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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define __USE_XOPEN
#include <math.h>

#include "formant.h"
#include "processing.h"

#ifdef LIBFORMANT_TEST
#include "greatest.h"
#endif

/* Here are the major fudge factors for tweaking the formant tracker. */
/* maximum number of candidate mappings allowed */
enum { MAX_CANDIDATES = 300 };
/* equivalent delta-Hz cost for missing formant */
static const double MISSING = 1;
/* equivalent bandwidth cost of a missing formant */
static const double NOBAND = 1000;
/* cost for proportional frequency changes */
/* with good "stationarity" function:*/
static const double DF_FACT = 20.0;
/* cost for proportional dev. from nominal freqs. */
static const double DFN_FACT = 0.3;
/* cost per Hz of bandwidth in the poles */
static const double BAND_FACT = 0.002;
/* bias toward selecting low-freq. poles */
static const double F_BIAS = 0.000;
/* cost of mapping f1 and f2 to same frequency */
static const double F_MERGE = 2000.0;

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
        .sample_rate = 0,
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
static int canbe(const double *fmins, const double *fmaxs, const double *fre,
                 int pnumb, int fnumb)
{
    return fre[pnumb] >= fmins[fnumb] && fre[pnumb] <= fmaxs[fnumb];
}

/* This does the real work of mapping frequencies to formants. */
/* cand: candidate number being considered */
/* pnumb: pole number under consideration */
/* fnumb: formant number under consideration */
/* maxp: number of poles to consider */
/* maxf: number of formants to find */
static int candy(int **pc, double *fre, int maxp, int maxf, bool domerge,
                 int ncan, int cand, int pnumb, int fnumb,
                 const double *fmins, const double *fmaxs)
{
    int i,j;

    if (fnumb < maxf)
        pc[cand][fnumb] = -1;

    if (pnumb < maxp && fnumb < maxf) {
        if (canbe(fmins, fmaxs, fre, pnumb,fnumb)) {
            pc[cand][fnumb] = pnumb;

            /* allow for f1,f2 merger */
            if (domerge && fnumb == 0 && canbe(fmins, fmaxs, fre, pnumb, fnumb + 1)) {
                ncan++;
                pc[ncan][0] = pc[cand][0];

                /* same pole, next formant */
                ncan = candy(pc, fre, maxp, maxf, domerge, ncan, ncan, pnumb,
                             fnumb + 1, fmins, fmaxs);
            }

            /* next formant; next pole */
            ncan = candy(pc, fre, maxp, maxf, domerge, ncan, cand, pnumb + 1,
                         fnumb + 1, fmins, fmaxs);

            /* try other frequencies for this formant */
            if (pnumb + 1 < maxp && canbe(fmins, fmaxs, fre, pnumb + 1, fnumb)) {
                /* add one to the candidate index/tally */
                ncan++;

                /* clone the lower formants */
                for (i = 0; i < fnumb; i++)
                    pc[ncan][i] = pc[cand][i];

                ncan = candy(pc, fre, maxp, maxf, domerge, ncan, ncan,
                             pnumb + 1, fnumb, fmins, fmaxs);
            }
        } else {
            ncan = candy(pc, fre, maxp, maxf, domerge, ncan, cand,
                         pnumb + 1, fnumb, fmins, fmaxs);
        }
    }

    /* If all pole frequencies have been examined without finding one which
       will map onto the current formant, go on to the next formant leaving the
       current formant null. */
    if (pnumb >= maxp && fnumb < maxf - 1 && pc[cand][fnumb] < 0) {
        if (fnumb) {
            j = fnumb - 1;

            while (j > 0 && pc[cand][j] < 0)
                j -= 1;

            j = pc[cand][j];
            i = j >= 0 ? j : 0;
        } else {
            i = 0;
        }

        ncan = candy(pc, fre, maxp, maxf, domerge, ncan, cand, i, fnumb + 1,
                     fmins, fmaxs);
    }

    return ncan;
}

/* Given a set of pole frequencies and allowable formant frequencies
   for nform formants, calculate all possible mappings of pole frequencies
   to formants, including, possibly, mappings with missing formants. */
/* freq: poles ordered by increasing FREQUENCY */
static int get_fcand(int npole, double *freq, int nform, int **pcan,
                     bool domerge, const double *fmins, const double *fmaxs)
{
    return candy(pcan, freq, npole, nform, domerge, 0, 0, 0, 0, fmins, fmaxs) + 1;
}

static void pole_dpform(pole_t *pole, const sound_t *ps, formants_t *f) {
    double minerr, ftemp, berr, ferr, bfact, ffact, fbias, merger=0.0;
    double merge_cost, FBIAS;
    int	ic, mincan=0;
    int	**pcan;
    int dmaxc,dminc,dcountf;
    bool domerge;

    /*  "nominal" freqs.*/
    double fnom[]  = { 500, 1500, 2500, 3500, 4500, 5500, 6500};
    /* frequency bounds */
    double fmins[] = {  50,  400, 1000, 2000, 2000, 3000, 3000};
    /* for 1st 5 formants */
    double fmaxs[] = {1500, 3500, 4500, 5000, 6000, 6000, 8000};

    if (NOM_FREQ > 0.0) {
        for (size_t i = 0; i < MAX_FORMANTS; i++) {
            fnom[i] = (i * 2 + 1) * NOM_FREQ;
            fmins[i] = fnom[i] - (i + 1) * NOM_FREQ + 50.0;
            fmaxs[i] = fnom[i] + i * NOM_FREQ + 1000.0;
        }
    }

    FBIAS = F_BIAS / (0.01 * ps->sample_count);

    /* Setup working values of the cost weights. */
    bfact = BAND_FACT /(0.01 * ps->sample_count);
    ffact = DFN_FACT /(0.01 * ps->sample_count);

    merge_cost = F_MERGE;

    if (merge_cost > 1000.0)
        domerge = false;

    /* Allocate space for the formant and bandwidth arrays to be passed back. */
    double fr[FORMANT_COUNT];

    /* Allocate space for the raw candidate array. */
    pcan = malloc(sizeof(*pcan) * MAX_CANDIDATES);

    for (size_t i = 0; i < MAX_CANDIDATES; i++)
        pcan[i] = malloc(sizeof(**pcan) * FORMANT_COUNT);

    /* Allocate space for the dp lattice */
    dp_lattice_t fl;

    /* main formant tracking loop */
    size_t ncan = 0;

    /* Get all likely mappings of the poles onto formants for this frame. */
    /* if there ARE pole frequencies available... */
    if (pole->npoles) {
        ncan = get_fcand(pole->npoles, pole->freq, FORMANT_COUNT, pcan, domerge,
                         fmins, fmaxs);

        /* Allocate space for this frame's candidates in the dp lattice. */
        fl.prept = malloc(sizeof(*fl.prept) * ncan);
        fl.cumerr = malloc(sizeof(double) * ncan);
        fl.cand = malloc(sizeof(*fl.cand) * ncan);

        /* allocate cand. slots and install candidates */
        for (size_t j = 0; j < ncan; j++) {
            fl.cand[j] = malloc(sizeof(**fl.cand) * FORMANT_COUNT);

            for (size_t k = 0; k < FORMANT_COUNT; k++)
                fl.cand[j][k] = pcan[j][k];
        }
    }

    fl.ncand = ncan;

    /* compute the distance between the current and previous mappings */
    for (size_t j = 0; j < ncan; j++) {	/* for each CURRENT mapping... */
        minerr = 0;

        /* point to best previous mapping */
        /* (Note that mincan=-1 if there were no candidates in prev. fr.) */
        /* Compute the local costs for this current mapping. */
        fl.prept[j] = mincan;

        berr = 0;
        ferr = 0;
        fbias = 0;

        for (size_t k = 0; k < FORMANT_COUNT; k++) {
            ic = fl.cand[j][k];

            if (ic >= 0) {
                /* F1 candidate? */
                if (!k) {
                    ftemp = pole->freq[ic];
                    merger = domerge && ftemp == pole->freq[fl.cand[j][1]]
                        ? merge_cost
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
        fl.cumerr[j] = FBIAS * fbias + bfact * berr + merger +
            ffact * ferr + minerr;
    }

    /* Pick the candidate in the final frame with the lowest cost. */
    /* Starting with that min.-cost cand., work back thru the lattice. */
    dmaxc = 0;
    dminc = 100;
    dcountf = 0;
    mincan = -1;

    /* have candidates at this frame? */
    if (fl.ncand) {
        minerr = fl.cumerr[0];
        mincan = 0;

        for (size_t j = 1; j < fl.ncand; j++) {
            if (fl.cumerr[j] < minerr) {
                minerr = fl.cumerr[j];
                mincan = j;
            }
        }
    }

    /* if there is a "best" candidate at this frame */
    /* note that mincan will remain =-1 if no candidates */
    if (mincan >= 0) {
        int j = fl.ncand;

        if (j > dmaxc)
            dmaxc = j;
        else if (j < dminc)
            dminc = j;

        dcountf++;

        for (size_t j = 0; j < FORMANT_COUNT; j++) {
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
        for (size_t j = 0; j < FORMANT_COUNT; j++)
            fr[j] = fnom[j];
    }

    /* Deallocate all the DP lattice work space. */
    if (fl.ncand) {
        if (fl.cand) {
            for (size_t j = 0; j < fl.ncand; j++)
                free(fl.cand[j]);

            free(fl.cand);
            free(fl.cumerr);
            free(fl.prept);
        }
    }

    /* Deallocate space for the raw candidate aray. */
    for (size_t i = 0; i < MAX_CANDIDATES; i++)
        free(pcan[i]);

    free(pcan);

    f->f1 = fr[0];
    f->f2 = fr[1];
}

static void pole_lpc(pole_t *pole, const sound_t *sp) {
#define X (M_PI / (LPC_ORDER + 1))

    double lpca[LPC_ORDER_MAX];
    double rr[LPC_ORDER_MAX], ri[LPC_ORDER_MAX];
    double flo;

    lpc(sp->sample_count, sp->samples, lpca, &pole->rms);

    /* set up starting points for the root search near unit circle */
    for (size_t i = 0; i <= LPC_ORDER; i += 1) {
        flo = LPC_ORDER - i;
        rr[i] = 2.0 * cos((flo + 0.5) * X);
        ri[i] = 2.0 * sin((flo + 0.5) * X);
    }

    /* don't waste time on low energy frames */
    if (pole->rms > 1.0) {
        formant(sp->sample_rate, lpca, &pole->npoles, pole->freq, pole->band,
            rr, ri);
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
   passband gain.  This filter is convolved with the signal in buf.
   The output is placed in buf2. */
static void fir(const formant_sample_t *buf, int in_samps, formant_sample_t *bufo,
                int ncoef, formant_sample_t *ic)
{
    enum { M = 15 };
    enum { L = 1 << 14 };

    const size_t lcoef = ncoef - 1;
    formant_sample_t *buft, *bufp, *bufp2;
    formant_sample_t co[256], mem[256];

    int integral;

    bufp = &ic[lcoef];
    bufp2 = co;
    buft = &co[lcoef * 2];
    integral = 0;

    for (size_t i = 0; i < lcoef; i += 1) {
        integral += ic[lcoef - i];
        co[i] = co[lcoef * 2 - i] = -ic[lcoef - i];
    }

    integral *= 2;
    integral += *bufp;
    co[lcoef] = integral - *bufp;

    buft = mem;

    for (size_t i = 0; i < lcoef; i += 1)
        buft[i] = 0;

    for (size_t i = lcoef; i < (size_t) 2 * ncoef - 1; i += 1)
        buft[i] = *buf++;

    const int k = (ncoef << 1) - 1;

    for (int i = in_samps - ncoef; i > 0; i -= 1) {
        buft = mem;
        bufp = co;
        bufp2 = mem + 1;
        int sum = 0;

        for (int j = k; j > 0; j -= 1) {
            sum += (*bufp * *buft + L) >> M;
            *buft = *bufp2;

            bufp += 1;
            buft += 1;
            bufp2 += 1;
        }

        buft -= 1;

        *buft = *buf;		/* new data to memory */
        *bufo = sum;

        buf += 1;
        bufo += 1;
    }

    for (int i = ncoef; i > 0; i -= 1) {	/* pad data end with zeros */
        buft = mem;
        bufp = co;
        bufp2 = mem + 1;
        int sum = 0;

        for (int j = k; j > 0; j -= 1) {
            sum += (*bufp * *buft + L) >> M;
            *buft = *bufp2;

            bufp += 1;
            buft += 1;
            bufp2 += 1;
        }

        *--buft = 0;
        *bufo++ = sum;
    }
}

void sound_highpass(sound_t *s) {
    /* This assumes the sampling frequency is 10kHz and that the FIR
       is a Hanning function of (LCSIZ/10)ms duration. */
    enum { LCSIZ = 101 };
    enum { LEN = LCSIZ / 2 + 1 };

#define FN (M_PI * 2.0 / (LCSIZ - 1))
#define SCALE (32767.0 / (.5 * LCSIZ))

    formant_sample_t dataout[s->sample_count];
    formant_sample_t lcf[LCSIZ];

    for (size_t i = 0; i < LEN; i += 1)
        lcf[i] = SCALE * (0.5 + 0.4 * cos(FN * (double) i));

    fir(s->samples, s->sample_count, dataout, LEN, lcf);
    memcpy(s->samples, dataout, sizeof(formant_sample_t) * s->sample_count);

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
