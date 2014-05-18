// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014  Formant Industries, Inc.
//     Copyright (c) 1990-1994  Entropic Research Laboratory, Inc.
//         All rights reserved
//     Copyright (c) 1987-1990  AT&T, Inc.
//     Copyright (c) 1986-1990  Entropic Speech, Inc.
//
// Written by: David Talkin
// Revised by: John Shore

#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __USE_XOPEN
#include <math.h>

#include "formant.h"

#ifdef LIBFORMANT_TEST
#include "greatest.h"
#endif

#define FORMANT_SAMPLE_RATE 10000

#define FORMANT_COUNT 4
#define MAX_FORMANTS 7

#define LPC_STABLE 70
#define LPC_ORDER 12
#define LPC_ORDER_MIN 2
#define LPC_ORDER_MAX 30
#define LPC_COEF (LPC_ORDER + 1)

// Nominal F1 frequency.
#define NOM_FREQ 0

static_assert(LPC_ORDER <= LPC_ORDER_MAX && LPC_ORDER >= LPC_ORDER_MIN,
    "LPC_ORDER out of bounds");

static_assert(FORMANT_COUNT <= MAX_FORMANTS,
    "FORMANT_COUNT out of bounds");

static_assert(FORMANT_COUNT <= (LPC_ORDER - 4) / 2,
    "FORMANT_COUNT too large");

/* Here are the major fudge factors for tweaking the formant tracker. */
/* maximum number of candidate mappings allowed */
#define MAX_CANDIDATES 300
/* equivalent delta-Hz cost for missing formant */
#define MISSING 1
/* equivalent bandwidth cost of a missing formant */
#define NOBAND 1000
/* cost for proportional frequency changes */
/* with good "stationarity" function:*/
#define DF_FACT 20.0
/* cost for proportional dev. from nominal freqs. */
#define DFN_FACT 0.3
/* cost per Hz of bandwidth in the poles */
#define BAND_FACT 0.002
/* bias toward selecting low-freq. poles */
#define F_BIAS 0.000
/* cost of mapping f1 and f2 to same frequency */
#define F_MERGE 2000.0
#define DO_MERGE (F_MERGE <= 1000.0)

typedef struct { /* structure of a DP lattice node for formant tracking */
    size_t ncand; /* # of candidate mappings for this frame */
    int **cand;      /* pole-to-formant map-candidate array */
    int *prept;	 /* backpointer array for each frame */
    double *cumerr;	 /* cum. errors associated with each cand. */
} dp_lattice_t;

typedef struct {   /* structure to hold raw LPC analysis data */
    double rms;    /* rms for current LPC analysis frame */
    double rms2;    /* rms for current F0 analysis frame */
    double f0;     /* fundamental frequency estimate for this frame */
    double pv;		/* probability that frame is voiced */
    size_t npoles; /* # of complex poles from roots of LPC polynomial */
    double freq[LPC_ORDER];  /* array of complex pole frequencies (Hz) */
    double band[LPC_ORDER];  /* array of complex pole bandwidths (Hz) */
} pole_t;

void sound_init(sound_t *s) {
    *s = (sound_t) {
        .sample_count = 0,
        .samples = NULL,
    };
}

void sound_destroy(sound_t *s) {
    free(s->samples);
}

void sound_resize(sound_t *s, size_t sample_count) {
    if (sample_count <= s->sample_count)
        return;

    s->sample_count = sample_count;
    s->samples = realloc(s->samples, sample_count * sizeof(formant_sample_t));
    assert(s->samples);
}

/*
 *
 *	An implementation of the Le Roux - Gueguen PARCOR computation.
 *	See: IEEE/ASSP June, 1977 pp 257-259.
 *
 *	Author: David Talkin	Jan., 1984
 *
 */

/*
 * Compute the pp+1 autocorrelation lags of the windowsize samples in s.
 * Return the normalized autocorrelation coefficients in coef.
 * The rms is returned in rms.
 */
static void autoc(size_t windowsize, const formant_sample_t *s, double *coef,
                  size_t ncoef, double *rms)
{
    double sum0 = 0;

    for (size_t i = 0; i < windowsize; i += 1)
        sum0 += s[i] * s[i];

    /* coef[0] will always =1. */
    coef[0] = 1;

    /* No energy: fake low-energy white noise. */
    if (sum0 == 0) {
        /* Arbitrarily assign 1 to rms. */
        *rms = 1;

        /* Now fake autocorrelation of white noise. */
        for (size_t i = 1; i < ncoef; i += 1)
            coef[i] = 0;

        return;
    }

    for (size_t i = 1; i < ncoef; i += 1) {
        double sum = 0;

        for (size_t j = 0; j < windowsize - i; j += 1)
            sum += s[j] * s[i + j];

        coef[i] = sum / sum0;
    }

    *rms = sqrt(sum0 / (double) windowsize);
}

/*
 * Compute the AR and PARCOR coefficients using Durbin's recursion.
 * Note: Durbin returns the coefficients in normal sign format.
 *	(i.e. a[0] is assumed to be = +1.)
 */
static void durbin(const double *coef, double *a, size_t ncoef) {
    double b[LPC_COEF];
    double k;
    double e, s;

    e = coef[0];
    k = -coef[1] / e;
    a[0] = k;
    e *= 1 - k*k;

    for (size_t i = 1; i < ncoef - 1; i += 1) {
        s = 0;

        for (size_t j = 0; j < i; j += 1)
            s -= a[j] * coef[i-j];

        k = (s - coef[i+1]) / e;
        a[i] = k;

        for (size_t j = 0; j <= i; j += 1)
            b[j] = a[j];

        for (size_t j = 0; j < i; j += 1)
            a[j] += k * b[i-j-1];

        e *= 1 - k*k;
    }
}

static void lpc(size_t wsize, const formant_sample_t *data, double *lpca,
                double *rms)
{
    double rho[LPC_COEF];

    autoc(wsize, data, rho, LPC_COEF, rms);

    /* add a little to the diagonal for stability */
    if (LPC_STABLE > 1.0) {
        double ffact = 1.0 / (1.0 + exp(-LPC_STABLE / 20.0 * log(10.0)));

        for (size_t i = 1; i < LPC_COEF; i += 1)
            rho[i] *= ffact;
    }

    durbin(rho, &lpca[1], LPC_COEF);

    lpca[0] = 1.0;
}

/*		lbpoly.c		*/
/*					*/
/* A polynomial root finder using the Lin-Bairstow method (outlined
   in R.W. Hamming, "Numerical Methods for Scientists and
   Engineers," McGraw-Hill, 1962, pp 356-359.)		*/

#define MAX_ITS	100	/* Max iterations before trying new starts */
#define MAX_TRYS	100	/* Max number of times to try new starts */
#define MAX_ERR		1.e-6	/* Max acceptable error in quad factor */

/* find x, where a*x**2 + b*x + c = 0   */
/* return real and imag. parts of roots */
static int qquad(double a, double b, double c, double *r1r, double *r1i,
                 double *r2r, double *r2i)
{
    double  numi, den, y;

    if (a == 0) {
        if (b == 0)
            return false;

        *r1r = -c / b;
        *r1i = *r2r = *r2i = 0;

        return true;
    }

    numi = b*b - 4.0 * a * c;

    if (numi >= 0) {
        /*
         * Two forms of the quadratic formula:
         *  -b + sqrt(b^2 - 4ac)           2c
         *  ------------------- = --------------------
         *           2a           -b - sqrt(b^2 - 4ac)
         * The r.h.s. is numerically more accurate when
         * b and the square root have the same sign and
         * similar magnitudes.
         */
        *r1i = *r2i = 0;

        if (b < 0) {
            y = -b + sqrt(numi);
            *r1r = y / (2.0 * a);
            *r2r = 2.0 * c / y;
        } else {
            y = -b - sqrt(numi);
            *r1r = 2.0 * c / y;
            *r2r = y / (2.0 * a);
        }
    } else {
        den = 2.0 * a;
        *r1i = sqrt(-numi) / den;
        *r2i = -*r1i;
        *r2r = *r1r = -b / den;
    }

    return true;
}

/* return false on error */
/* a: coeffs. of the polynomial (increasing order) */
/* order: the order of the polynomial */
/* rootr, rooti: the real and imag. roots of the polynomial */
/* Rootr and rooti are assumed to contain starting points for the root
   search on entry to lbpoly(). */
static int lbpoly(double *a, int order, double *rootr, double *rooti) {
    int	    ord, itcnt, k, ntrys;
    double  delp, delq, den;
    double b[LPC_COEF];
    double c[LPC_COEF];
    const double lim0 = 0.5 * sqrt(DBL_MAX);

    for (ord = order; ord > 2; ord -= 2) {
        const int ordm1 = ord - 1;
        const int ordm2 = ord - 2;

        /* Here is a kluge to prevent UNDERFLOW! (Sometimes the near-zero
           roots left in rootr and/or rooti cause underflow here...	*/
        if (fabs(rootr[ordm1]) < 1.0e-10)
            rootr[ordm1] = 0.0;

        if (fabs(rooti[ordm1]) < 1.0e-10)
            rooti[ordm1] = 0.0;

        /* set initial guesses for quad factor */
        double p = -2.0 * rootr[ordm1];
        double q = rootr[ordm1]*rootr[ordm1] + rooti[ordm1]*rooti[ordm1];

        for (ntrys = 0; ntrys < MAX_TRYS; ntrys += 1) {
            int	found = false;

            for (itcnt = 0; itcnt < MAX_ITS; itcnt += 1) {
                double lim = lim0 / (1 + fabs(p) + fabs(q));

                b[ord] = a[ord];
                b[ordm1] = a[ordm1] - (p * b[ord]);

                c[ord] = b[ord];
                c[ordm1] = b[ordm1] - (p * c[ord]);

                for (k = 2; k <= ordm1; k += 1) {
                    int mmk = ord - k;
                    int mmkp2 = mmk+2;
                    int mmkp1 = mmk+1;

                    b[mmk] = a[mmk] - (p* b[mmkp1]) - (q* b[mmkp2]);
                    c[mmk] = b[mmk] - (p* c[mmkp1]) - (q* c[mmkp2]);

                    if (b[mmk] > lim || c[mmk] > lim)
                        break;
                }

                /* normal exit from for (k ... */
                if (k > ordm1) {
                    b[0] = a[0] - p * b[1] - q * b[2];

                    if (b[0] <= lim)
                        k += 1;
                }

                /* Some coefficient exceeded lim; */
                /* potential overflow below. */
                if (k <= ord)
                    break;

                const double err = fabs(b[0]) + fabs(b[1]);

                if (err <= MAX_ERR) {
                    found = true;
                    break;
                }

                den = c[2] * c[2] - c[3] * (c[1] - b[1]);

                if (den == 0.0)
                    break;

                delp = c[2] * b[1] - c[3] * b[0];
                delp /= den;

                delq = c[2] * b[0] - b[1] * (c[1] - b[1]);
                delq /= den;

                p += delp;
                q += delq;

            }

            /* we finally found the root! */
            if (found)
                break;

            /* try some new starting values */
            p = ((double) rand() - 0.5 * RAND_MAX) / (double) RAND_MAX;
            q = ((double) rand() - 0.5 * RAND_MAX) / (double) RAND_MAX;

        }

        if (itcnt >= MAX_ITS && ntrys >= MAX_TRYS)
            return false;

        if (!qquad(1.0, p, q, &rootr[ordm1], &rooti[ordm1], &rootr[ordm2],
                   &rooti[ordm2]))
            return false;

        /* Update the coefficient array with the coeffs. of the
           reduced polynomial. */
        for (int i = 0; i <= ordm2; i += 1)
            a[i] = b[i + 2];
    }

    /* Is the last factor a quadratic? */
    if (ord == 2)
        return qquad(a[2], a[1], a[0], &rootr[1], &rooti[1], &rootr[0],
                     &rooti[0]);

    if (ord < 1)
        return false;

    if (a[1] != 0.0)
        rootr[0] = -a[0] / a[1];
    else
        /* arbitrary recovery value */
        rootr[0] = 100.0;

    rooti[0] = 0.0;

    return true;
}

/* Find the roots of the LPC denominator polynomial and convert the z-plane
   zeros to equivalent resonant frequencies and bandwidths.	*/
/* The complex poles are then ordered by frequency.  */
/* lpc_ord: order of the LP model */
/* n_form: number of COMPLEX roots of the LPC polynomial */
/* init: preset to true if no root candidates are available */
/* lpca: linear predictor coefficients */
/* freq: returned array of candidate formant frequencies */
/* band: returned array of candidate formant bandwidths */
static size_t formant(double *lpca, double *freq, double *band, double *rr,
                      double *ri)
{
#define PI_2T (M_PI * 2.0 / FORMANT_SAMPLE_RATE)
/* hold the folding frequency. */
#define THETA (FORMANT_SAMPLE_RATE / 2.0)

    double  copy;

    /* was there a problem in the root finder? */
    if (!lbpoly(lpca, LPC_ORDER, rr, ri))
        return 0;

    size_t fc = 0;

    /* convert the z-plane locations to frequencies and bandwidths */
    for (size_t i = 0; i < LPC_ORDER; i += 1) {
        if (rr[i] == 0.0 && ri[i] == 0.0)
            continue;

        const double theta = atan2(ri[i], rr[i]);
        freq[fc] = fabs(theta / PI_2T);
        band[fc] = 0.5 * FORMANT_SAMPLE_RATE * log(rr[i]*rr[i] + ri[i]*ri[i]) / M_PI;

        if (band[fc] < 0.0)
            band[fc] = -band[fc];

        /* Count the number of real and complex poles. */
        fc += 1;

        /* complex pole? */
        /* if so, don't duplicate */
        if (rr[i] == rr[i + 1] && ri[i] == -ri[i + 1] && ri[i] != 0.0)
            i += 1;
    }

    /* Now order the complex poles by frequency.  Always place the (uninteresting)
       real poles at the end of the arrays.	*/

    /* order the poles by frequency (bubble) */
    for (size_t i = 0; i < fc - 1; i += 1) {
        for (size_t j = 0; j < fc - 1 - i; j += 1) {
            /* Force the real poles to the end of the list. */
            bool iscomp1 = freq[j] > 1.0 && freq[j] < THETA;
            bool iscomp2 = freq[j + 1] > 1.0 && freq[j + 1] < THETA;
            bool swit = freq[j] > freq[j + 1] && iscomp2;

            if (!(swit || (iscomp2 && !iscomp1)))
                continue;

            copy = band[j + 1];
            band[j + 1] = band[j];
            band[j] = copy;

            copy = freq[j + 1];
            freq[j + 1] = freq[j];
            freq[j] = copy;
        }
    }

    size_t nform = 0;

    /* Now count the complex poles as formant candidates. */
    for (size_t i = 0; i < fc; i += 1)
        if (freq[i] > 1.0 && freq[i] < THETA - 1)
            nform += 1;

    return nform;

#undef THETA
#undef PI_2T
}

/* A formant tracker based on LPC polynomial roots and dynamic programming */
/* At each frame, the LPC poles are ordered by increasing frequency.  All
   "reasonable" mappings of the poles to F1, F2, ... are performed.
   The cost of "connecting" each of these mappings with each of the mappings
   in the previous frame is computed.  The lowest cost connection is then
   chosen as the optimum one.  At each frame, each mapping has associated
   with it a cost based on the formant bandwidths and frequencies.  This
   "local" cost is finally added to the cost of the best "connection."  At
   end of utterance (or after a reasonable delay like .5sec) the best
   mappings for the entire utterance may be found by retracing back through
   best candidate mappings, starting at end of utterance (or current frame).
   */

/* can this pole be this freq.? */
static bool canbe(const double *fmins, const double *fmaxs, const double *fre,
                  size_t pnumb, size_t fnumb)
{
    return fre[pnumb] >= fmins[fnumb] && fre[pnumb] <= fmaxs[fnumb];
}

/* This does the real work of mapping frequencies to formants. */
/* cand: candidate number being considered */
/* pnumb: pole number under consideration */
/* fnumb: formant number under consideration */
/* maxp: number of poles to consider */
static size_t candy(int **pc, const double *fre, size_t maxp,
                    size_t ncan, size_t cand, size_t pnumb, size_t fnumb,
                    const double *fmins, const double *fmaxs)
{
    if (fnumb < FORMANT_COUNT)
        pc[cand][fnumb] = -1;

    if (pnumb < maxp && fnumb < FORMANT_COUNT) {
        if (canbe(fmins, fmaxs, fre, pnumb, fnumb)) {
            pc[cand][fnumb] = (int) pnumb;

            /* allow for f1,f2 merger */
            if (DO_MERGE) {
                if (fnumb == 0 && canbe(fmins, fmaxs, fre, pnumb, fnumb + 1)) {
                    ncan += 1;
                    pc[ncan][0] = pc[cand][0];

                    /* same pole, next formant */
                    ncan = candy(pc, fre, maxp, ncan, ncan, pnumb,
                                 fnumb + 1, fmins, fmaxs);
                }
            }

            /* next formant; next pole */
            ncan = candy(pc, fre, maxp, ncan, cand, pnumb + 1,
                         fnumb + 1, fmins, fmaxs);

            /* try other frequencies for this formant */
            if (pnumb + 1 < maxp && canbe(fmins, fmaxs, fre, pnumb + 1, fnumb)) {
                /* add one to the candidate index/tally */
                ncan += 1;

                /* clone the lower formants */
                for (size_t i = 0; i < fnumb; i += 1)
                    pc[ncan][i] = pc[cand][i];

                ncan = candy(pc, fre, maxp, ncan, ncan,
                             pnumb + 1, fnumb, fmins, fmaxs);
            }
        } else {
            ncan = candy(pc, fre, maxp, ncan, cand,
                         pnumb + 1, fnumb, fmins, fmaxs);
        }
    }

    /* If all pole frequencies have been examined without finding one which
       will map onto the current formant, go on to the next formant leaving the
       current formant null. */
    if (pnumb >= maxp && fnumb < FORMANT_COUNT - 1 && pc[cand][fnumb] < 0) {
        size_t i;

        if (fnumb) {
            int j = (int) fnumb - 1;

            while (j > 0 && pc[cand][j] < 0)
                j -= 1;

            int p = pc[cand][j];
            i = p >= 0 ? (size_t) p : 0;
        } else {
            i = 0;
        }

        ncan = candy(pc, fre, maxp, ncan, cand, i, fnumb + 1,
                     fmins, fmaxs);
    }

    return ncan;
}

/* Given a set of pole frequencies and allowable formant frequencies
   for nform formants, calculate all possible mappings of pole frequencies
   to formants, including, possibly, mappings with missing formants. */
/* freq: poles ordered by increasing FREQUENCY */
static size_t get_fcand(size_t npole, double *freq, int **pcan, const double *fmins,
                        const double *fmaxs)
{
    return candy(pcan, freq, npole, 0, 0, 0, 0, fmins, fmaxs) + 1;
}

static void pole_dpform(pole_t *pole, const sound_t *ps, formants_t *f) {
    double minerr, berr, ferr, bfact, ffact, fbias, merger=0.0;
    int	ic;
    int	**pcan;

    /*  "nominal" freqs.*/
    double fnom[MAX_FORMANTS]  = { 500, 1500, 2500, 3500, 4500, 5500, 6500};
    /* frequency bounds */
    double fmins[MAX_FORMANTS] = {  50,  400, 1000, 2000, 2000, 3000, 3000};
    /* for 1st 5 formants */
    double fmaxs[MAX_FORMANTS] = {1500, 3500, 4500, 5000, 6000, 6000, 8000};

    if (NOM_FREQ > 0.0) {
        for (size_t i = 0; i < MAX_FORMANTS; i += 1) {
            fnom[i] = (i * 2 + 1) * NOM_FREQ;
            fmins[i] = fnom[i] - ((double) i + 1) * NOM_FREQ + 50.0;
            fmaxs[i] = fnom[i] + (double) i * NOM_FREQ + 1000.0;
        }
    }

    /* Setup working values of the cost weights. */
    bfact = BAND_FACT / (0.01 * (double) ps->sample_count);
    ffact = DFN_FACT / (0.01 * (double) ps->sample_count);

    /* Allocate space for the formant and bandwidth arrays to be passed back. */
    double fr[FORMANT_COUNT];

    /* Allocate space for the raw candidate array. */
    pcan = malloc(sizeof(*pcan) * MAX_CANDIDATES);

    for (size_t i = 0; i < MAX_CANDIDATES; i += 1)
        pcan[i] = malloc(sizeof(**pcan) * FORMANT_COUNT);

    /* Allocate space for the dp lattice */
    dp_lattice_t fl;

    /* main formant tracking loop */
    size_t ncan = 0;

    /* Get all likely mappings of the poles onto formants for this frame. */
    /* if there ARE pole frequencies available... */
    if (pole->npoles) {
        ncan = get_fcand(pole->npoles, pole->freq, pcan, fmins, fmaxs);

        /* Allocate space for this frame's candidates in the dp lattice. */
        fl.prept = malloc(sizeof(*fl.prept) * ncan);
        fl.cumerr = malloc(sizeof(double) * ncan);
        fl.cand = malloc(sizeof(*fl.cand) * ncan);

        /* allocate cand. slots and install candidates */
        for (size_t j = 0; j < ncan; j += 1) {
            fl.cand[j] = malloc(sizeof(**fl.cand) * FORMANT_COUNT);

            for (size_t k = 0; k < FORMANT_COUNT; k += 1)
                fl.cand[j][k] = pcan[j][k];
        }
    }

    fl.ncand = ncan;

    /* compute the distance between the current and previous mappings */
    for (size_t j = 0; j < ncan; j += 1) {	/* for each CURRENT mapping... */
        minerr = 0;

        /* point to best previous mapping */
        /* (Note that mincan=-1 if there were no candidates in prev. fr.) */
        /* Compute the local costs for this current mapping. */
        fl.prept[j] = 0;

        berr = 0;
        ferr = 0;
        fbias = 0;

        for (size_t k = 0; k < FORMANT_COUNT; k += 1) {
            ic = fl.cand[j][k];

            if (ic >= 0) {
                /* F1 candidate? */
                if (!k) {
                    merger = DO_MERGE && pole->freq[ic]== pole->freq[fl.cand[j][1]]
                        ? F_MERGE
                        : 0.0;
                }

                berr += pole->band[ic];
                ferr += fabs(pole->freq[ic] - fnom[k]) / fnom[k];
                fbias += pole->freq[ic];
            } else {
                /* if there was no freq. for this formant */
                fbias += fnom[k];
                berr += NOBAND;
                ferr += MISSING;
            }
        }

        /* Compute the total cost of this mapping and best previous. */
        fl.cumerr[j] = F_BIAS / (0.01 * (double) ps->sample_count) * fbias +
            bfact * berr + merger + ffact * ferr + minerr;
    }

    /* Pick the candidate in the final frame with the lowest cost. */
    /* Starting with that min.-cost cand., work back thru the lattice. */
    size_t dmaxc = 0;
    size_t dminc = 100;
    int dcountf = 0;
    int mincan = -1;

    /* have candidates at this frame? */
    if (fl.ncand) {
        minerr = fl.cumerr[0];
        mincan = 0;

        for (size_t j = 1; j < fl.ncand; j += 1) {
            if (fl.cumerr[j] < minerr) {
                minerr = fl.cumerr[j];
                mincan = (int) j;
            }
        }
    }

    /* if there is a "best" candidate at this frame */
    /* note that mincan will remain =-1 if no candidates */
    if (mincan >= 0) {
        if (fl.ncand > dmaxc)
            dmaxc = fl.ncand;
        else if (fl.ncand < dminc)
            dminc = fl.ncand;

        dcountf += 1;

        for (size_t j = 0; j < FORMANT_COUNT; j += 1) {
            int k = fl.cand[mincan][j];

            if (k >= 0)
                fr[j] = pole->freq[k];
            else
                /* IF FORMANT IS MISSING... */
                /* or insert neutral values */
                fr[j] = fnom[j];
        }

        mincan = fl.prept[mincan];
    } else {
        /* if no candidates, fake with "nominal" frequencies. */
        for (size_t j = 0; j < FORMANT_COUNT; j += 1)
            fr[j] = fnom[j];
    }

    /* Deallocate all the DP lattice work space. */
    if (fl.ncand) {
        if (fl.cand) {
            for (size_t j = 0; j < fl.ncand; j += 1)
                free(fl.cand[j]);

            free(fl.cand);
            free(fl.cumerr);
            free(fl.prept);
        }
    }

    /* Deallocate space for the raw candidate aray. */
    for (size_t i = 0; i < MAX_CANDIDATES; i += 1)
        free(pcan[i]);

    free(pcan);

    *f = (formants_t) {
        .f1 = (formant_sample_t) fr[0],
        .f2 = (formant_sample_t) fr[1],
    };
}

static void pole_lpc(pole_t *pole, const sound_t *sp) {
#define X (M_PI / (LPC_ORDER + 1))

    double lpca[LPC_ORDER_MAX];
    double rr[LPC_ORDER_MAX], ri[LPC_ORDER_MAX];
    double flo;

    lpc(sp->sample_count, sp->samples, lpca, &pole->rms);

    /* set up starting points for the root search near unit circle */
    for (size_t i = 0; i <= LPC_ORDER; i += 1) {
        flo = (double) LPC_ORDER - (double) i;
        rr[i] = 2.0 * cos((flo + 0.5) * X);
        ri[i] = 2.0 * sin((flo + 0.5) * X);
    }

    /* don't waste time on low energy frames */
    if (pole->rms > 1.0) {
        pole->npoles = formant(lpca, pole->freq, pole->band, rr, ri);
    } else {			/* write out no pole frequencies */
        pole->npoles = 0;
    }

#undef X
}

/*	Copyright (c) 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/* ic contains 1/2 the coefficients of a symmetric FIR filter with unity
   passband gain.  This filter is convolved in place with the given samples. */
static void fir(formant_sample_t *samples, size_t in_samps, size_t ncoef,
                const formant_sample_t *coef)
{
    enum { M = 15 };
    enum { L = 1 << 14 };

    const size_t lcoef = ncoef - 1;
    const size_t k = (ncoef * 2) - 1;

    formant_sample_t co[256], mem[256];

    int integral = 0;

    for (size_t i = 0; i < lcoef; i += 1)
        co[i] = (formant_sample_t) -coef[lcoef - i];

    for (size_t i = 0; i < lcoef; i += 1)
        co[lcoef * 2 - i] = co[i];

    for (size_t i = 1; i < lcoef; i += 1)
        integral += coef[i];

    co[lcoef] = (formant_sample_t)(integral * 2);

    for (size_t i = 0; i < lcoef; i += 1)
        mem[i] = 0;

    for (size_t i = 0; i < ncoef; i += 1)
        mem[lcoef + i] = samples[i];

    size_t in = ncoef;
    size_t out = 0;

    for (size_t i = 0; i < in_samps - ncoef; i += 1) {
        int sum = 0;

        for (size_t j = 0; j < k; j += 1) {
            sum += (co[j] * mem[j] + L) >> M;
            mem[j] = mem[j + 1];
        }

        mem[k - 1] = samples[in];		/* new data to memory */
        samples[out] = (formant_sample_t) sum;

        in += 1;
        out += 1;
    }

    for (size_t i = 0; i > ncoef; i += 1) {	/* pad data end with zeros */
        int sum = 0;

        for (size_t j = 0; j < k; j -= 1) {
            sum += (co[j] * mem[j] + L) >> M;
            mem[j] = mem[j + 1];
        }

        mem[k - 1] = 0;
        samples[out] = (formant_sample_t) sum;
        out += 1;
    }
}

void sound_highpass(sound_t *s) {
    /* This assumes the sampling frequency is 10kHz and that the FIR
       is a Hanning function of (LCSIZ/10)ms duration. */
    enum { LCSIZ = 101 };
    enum { LEN = LCSIZ / 2 + 1 };

#define FN (M_PI * 2.0 / (LCSIZ - 1))
#define SCALE (32767.0 / (.5 * LCSIZ))

    formant_sample_t lcf[LCSIZ];

    for (size_t i = 0; i < LEN; i += 1)
        lcf[i] = (formant_sample_t)(SCALE * (0.5 + 0.4 * cos(FN * (double) i)));

    fir(s->samples, s->sample_count, LEN, lcf);

#undef SCALE
#undef FN
}

void formants_calc(formants_t *f, const sound_t *s) {
    pole_t pole;

    pole_lpc(&pole, s);
    pole_dpform(&pole, s, f);
}

#ifdef LIBFORMANT_TEST
SUITE(formant_suite) {
}
#endif
