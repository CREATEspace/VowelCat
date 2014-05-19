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
#include <stdlib.h>
#include <string.h>

#define __USE_XOPEN
#include <math.h>

#include "formant.h"

#ifdef LIBFORMANT_TEST
#include "greatest.h"
#endif

// Required sample rate of input samples.
#define FORMANT_SAMPLE_RATE 10000

// Number of formants to calculate.
#define FORMANT_COUNT 4

// Factor used to stabilize the LPC diagonal.
#define LPC_STABLE 70

// Order of the LPC polynomial -- the number of terms.
#define LPC_ORDER 12
#define LPC_ORDER_MIN 2
#define LPC_ORDER_MAX 30
// Include the implicit first coefficient with value 1.
#define LPC_COEF (LPC_ORDER + 1)

// Nominal F1 frequency.
#define NOM_FREQ 0

// Maximum number of candidate mappings allowed.
#define MAX_CANDIDATES 64

// Equivalent delta-Hz cost for missing formant.
#define MISSING 1

// Equivalent bandwidth cost of a missing formant.
#define NOBAND 1000

// Cost for proportional frequency changes.
#define DF_FACT 20.0

// Cost for proportional dev. from nominal freqs.
#define DFN_FACT 30

// Cost per Hz of bandwidth in the poles.
#define BAND_FACT 0.2

// Bias toward selecting low-freq. poles.
#define F_BIAS 0.000

static_assert(LPC_ORDER <= LPC_ORDER_MAX && LPC_ORDER >= LPC_ORDER_MIN,
    "LPC_ORDER out of bounds");

static_assert(FORMANT_COUNT <= (LPC_ORDER - 4) / 2,
    "FORMANT_COUNT too large");

typedef int pcan_t[MAX_CANDIDATES][FORMANT_COUNT];

typedef struct { /* structure of a DP lattice node for formant tracking */
    size_t ncand; /* # of candidate mappings for this frame */
    pcan_t pcan;
    double cumerr[MAX_CANDIDATES];	 /* cum. errors associated with each cand. */
} dp_lattice_t;

typedef struct {   /* structure to hold raw LPC analysis data */
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
 * Return the rms if autocorrelation succeeded and 0 if not.
 */
static double autoc(size_t windowsize, const formant_sample_t *s, double *coef,
                    size_t ncoef)
{
    double sum0 = 0;

    for (size_t i = 0; i < windowsize; i += 1)
        sum0 += s[i] * s[i];

    /* coef[0] will always =1. */
    coef[0] = 1;

    /* No energy: fake low-energy white noise. */
    if (sum0 == 0) {
        /* Now fake autocorrelation of white noise. */
        for (size_t i = 1; i < ncoef; i += 1)
            coef[i] = 0;

        return 0.0;
    }

    for (size_t i = 1; i < ncoef; i += 1) {
        double sum = 0;

        for (size_t j = 0; j < windowsize - i; j += 1)
            sum += s[j] * s[i + j];

        coef[i] = sum / sum0;
    }

    return sqrt(sum0 / (double) windowsize);
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

static double lpc(size_t wsize, const formant_sample_t *data, double *lpca) {
    double rho[LPC_COEF];
    double rms = autoc(wsize, data, rho, LPC_COEF);

    /* add a little to the diagonal for stability */
    if (LPC_STABLE > 1.0) {
        double ffact = 1.0 / (1.0 + exp(-LPC_STABLE / 20.0 * log(10.0)));

        for (size_t i = 1; i < LPC_COEF; i += 1)
            rho[i] *= ffact;
    }

    durbin(rho, &lpca[1], LPC_COEF);

    lpca[0] = 1.0;

    return rms;
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
static bool qquad(double a, double b, double c, double *r1r, double *r1i,
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
static bool lbpoly(double *a, int order, double *rootr, double *rooti) {
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

            {
                double copy = band[j + 1];
                band[j + 1] = band[j];
                band[j] = copy;
            }

            {
                double copy = freq[j + 1];
                freq[j + 1] = freq[j];
                freq[j] = copy;
            }
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
static size_t candy(pcan_t pc, const double *fre, size_t maxp,
                    size_t ncan, size_t cand, size_t pnumb, size_t fnumb,
                    const double *fmins, const double *fmaxs)
{
    if (fnumb < FORMANT_COUNT)
        pc[cand][fnumb] = -1;

    if (pnumb < maxp && fnumb < FORMANT_COUNT) {
        if (canbe(fmins, fmaxs, fre, pnumb, fnumb)) {
            pc[cand][fnumb] = (int) pnumb;

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
        pnumb = 0;

        for (size_t i = fnumb; i; i -= 1) {
            if (pc[cand][i] >= 0) {
                pnumb = (size_t) pc[cand][i];
                break;
            }
        }

        ncan = candy(pc, fre, maxp, ncan, cand, pnumb, fnumb + 1,
                     fmins, fmaxs);
    }

    return ncan;
}

/* Given a set of pole frequencies and allowable formant frequencies
   for nform formants, calculate all possible mappings of pole frequencies
   to formants, including, possibly, mappings with missing formants. */
/* freq: poles ordered by increasing FREQUENCY */
static size_t get_fcand(size_t npole, double *freq, pcan_t pcan,
                        const double *fmins, const double *fmaxs)
{
    return candy(pcan, freq, npole, 0, 0, 0, 0, fmins, fmaxs) + 1;
}

static void pole_dpform(pole_t *pole, const sound_t *ps, formants_t *f) {
    /*  "nominal" freqs.*/
    double fnom[FORMANT_COUNT]  = { 500, 1500, 2500, 3500};
    /* frequency bounds */
    double fmins[FORMANT_COUNT] = {  50,  400, 1000, 2000};
    /* for 1st 5 formants */
    double fmaxs[FORMANT_COUNT] = {1500, 3500, 4500, 5000};

    if (NOM_FREQ > 0.0) {
        for (size_t i = 0; i < FORMANT_COUNT; i += 1) {
            fnom[i] = (i * 2 + 1) * NOM_FREQ;
            fmins[i] = fnom[i] - ((double) i + 1) * NOM_FREQ + 50.0;
            fmaxs[i] = fnom[i] + (double) i * NOM_FREQ + 1000.0;
        }
    }

    /* Setup working values of the cost weights. */
    const double bfact = BAND_FACT / (double) ps->sample_count;
    const double ffact = DFN_FACT / (double) ps->sample_count;

    /* Allocate space for the formant and bandwidth arrays to be passed back. */
    double fr[FORMANT_COUNT];

    /* Allocate space for the dp lattice */
    dp_lattice_t fl = {
        .ncand = 0,
    };

    /* Get all likely mappings of the poles onto formants for this frame. */
    /* if there ARE pole frequencies available... */
    if (pole->npoles)
        fl.ncand = get_fcand(pole->npoles, pole->freq, fl.pcan, fmins, fmaxs);

    assert(fl.ncand <= MAX_CANDIDATES);

    /* compute the distance between the current and previous mappings */
    for (size_t j = 0; j < fl.ncand; j += 1) {	/* for each CURRENT mapping... */
        /* (Note that mincan=-1 if there were no candidates in prev. fr.) */
        /* Compute the local costs for this current mapping. */
        double berr = 0;
        double ferr = 0;
        double fbias = 0;

        for (size_t k = 0; k < FORMANT_COUNT; k += 1) {
            int cand = fl.pcan[j][k];

            if (cand >= 0) {
                berr += pole->band[cand];
                ferr += fabs(pole->freq[cand] - fnom[k]) / fnom[k];
                fbias += pole->freq[cand];
            } else {
                /* if there was no freq. for this formant */
                berr += NOBAND;
                ferr += MISSING;
                fbias += fnom[k];
            }
        }

        /* Compute the total cost of this mapping and best previous. */
        fl.cumerr[j] = F_BIAS / (double) ps->sample_count * fbias +
            bfact * berr + ffact * ferr;
    }

    /* Pick the candidate in the final frame with the lowest cost. */
    /* Starting with that min.-cost cand., work back thru the lattice. */
    size_t mincan = 0;

    for (size_t j = 1; j < fl.ncand; j += 1)
        if (fl.cumerr[j] < fl.cumerr[mincan])
            mincan = j;

    /* if there is a "best" candidate at this frame */
    if (fl.ncand) {
        for (size_t j = 0; j < FORMANT_COUNT; j += 1) {
            if (fl.pcan[mincan][j] >= 0)
                fr[j] = pole->freq[fl.pcan[mincan][j]];
            else
                /* IF FORMANT IS MISSING... */
                /* or insert neutral values */
                fr[j] = fnom[j];
        }
    } else {
        /* if no candidates, fake with "nominal" frequencies. */
        for (size_t j = 0; j < FORMANT_COUNT; j += 1)
            fr[j] = fnom[j];
    }

    *f = (formants_t) {
        .f1 = (formant_sample_t) fr[0],
        .f2 = (formant_sample_t) fr[1],
    };
}

static void pole_lpc(pole_t *pole, const sound_t *sp) {
#define X (M_PI / (LPC_ORDER + 1))

    double rr[LPC_ORDER_MAX], ri[LPC_ORDER_MAX];

    /* set up starting points for the root search near unit circle */
    for (size_t i = 0; i <= LPC_ORDER; i += 1) {
        const double flo = (double)(LPC_ORDER - i);

        rr[i] = 2.0 * cos((flo + 0.5) * X);
        ri[i] = 2.0 * sin((flo + 0.5) * X);
    }

    double lpca[LPC_ORDER_MAX];
    double rms = lpc(sp->sample_count, sp->samples, lpca);

    /* don't waste time on low energy frames */
    if (rms > 0.0)
        pole->npoles = formant(lpca, pole->freq, pole->band, rr, ri);
    else			/* write out no pole frequencies */
        pole->npoles = 0;

#undef X
}

/*	Copyright (c) 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/* ic contains 1/2 the coefficients of a symmetric FIR filter with unity
   passband gain.  This filter is convolved in place with the given samples. */
static void fir(formant_sample_t *samples, const size_t nsamples,
                const size_t ncoef, const formant_sample_t *coef)
{
    enum { M = 15 };
    enum { L = 1 << 14 };

    const size_t lcoef = ncoef - 1;
    const size_t k = (ncoef * 2) - 1;

    formant_sample_t co[lcoef * 2], mem[ncoef * 2];

    size_t in = 0;
    size_t out = 0;

    for (size_t i = 0; i < lcoef; i += 1)
        co[i] = (formant_sample_t) -coef[lcoef - i];

    for (size_t i = 0; i < lcoef; i += 1)
        co[lcoef * 2 - i] = co[i];

    int integral = 0;

    for (size_t i = 1; i < lcoef; i += 1)
        integral += coef[i];

    co[lcoef] = (formant_sample_t)(integral * 2);

    for (size_t i = 0; i < lcoef; i += 1)
        mem[i] = 0;

    for (size_t i = lcoef; i < 2 * ncoef - 1; i += 1) {
        mem[i] = samples[in];
        in += 1;
    }

    for (size_t i = 0; i < nsamples - ncoef; i += 1) {
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
#define SCALE (32767.0 / LCSIZ * 2.0)

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
