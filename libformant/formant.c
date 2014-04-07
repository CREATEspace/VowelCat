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
    short **cand;      /* pole-to-formant map-candidate array */
    short *prept;	 /* backpointer array for each frame */
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
        .channel_count = 0,
        .sample_count = 0,
        .samples = NULL,
    };
}

void sound_reset(sound_t *s, size_t sample_rate, size_t channel_count) {
    s->sample_rate = sample_rate;
    s->channel_count = channel_count;
}

void sound_destroy(sound_t *s) {
    free(s->samples);
}

void sound_resize(sound_t *s, size_t sample_count) {
    if (sample_count > s->sample_count * s->channel_count)
        s->samples = realloc(s->samples, sample_count * sizeof(formant_sample_t));

    // The rest of the processing functions expect sample_count to be the number of
    // samples per channel.
    s->sample_count = sample_count / s->channel_count;
}

void sound_load_samples(sound_t *s, const formant_sample_t *samples, size_t sample_count) {
    sound_resize(s, sample_count);
    memcpy(s->samples, samples, sample_count * sizeof(formant_sample_t));
}

#ifdef LIBFORMANT_TEST
TEST test_sound_load_samples() {
    sound_t s;
    sound_init(&s);
    sound_reset(&s, 44100, 2);

    sound_load_samples(&s, NULL, 0);
    GREATEST_ASSERT_EQm("don't allocate with no samples", s.samples, NULL);
    GREATEST_ASSERT_EQm("don't set sample_count with no samples", s.sample_count, 0);

    formant_sample_t samples[8] = {0, 0, 0, 0, 0, 0, 0, 42};
    sound_load_samples(&s, samples, 8);
    GREATEST_ASSERT_EQm("set sample_count correctly", s.sample_count, 4);
    GREATEST_ASSERTm("samples allocated", s.samples);
    GREATEST_ASSERT_EQm("copy samples correctly", s.samples[7], 42);

    PASS();
}
#endif

// Get the i'th sample in the given channel.
static inline formant_sample_t sound_get_sample(const sound_t *s, size_t chan, size_t i) {
    return s->samples[i * s->channel_count + chan];
}

static inline void sound_set_sample(sound_t *s, size_t chan, size_t i,
                                    formant_sample_t val)
{
    s->samples[i * s->channel_count + chan] = val;
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
    return((fre[pnumb] >= fmins[fnumb])&&(fre[pnumb] <= fmaxs[fnumb]));
}

/* This does the real work of mapping frequencies to formants. */
/* cand: candidate number being considered */
/* pnumb: pole number under consideration */
/* fnumb: formant number under consideration */
/* maxp: number of poles to consider */
/* maxf: number of formants to find */
static int candy(short **pc, double *fre, int maxp, int maxf, bool domerge,
                 int ncan, int cand, int pnumb, int fnumb,
                 const double *fmins, const double *fmaxs)
{
    int i,j;

    if(fnumb < maxf) pc[cand][fnumb] = -1;
    if((pnumb < maxp)&&(fnumb < maxf)){
        if(canbe(fmins, fmaxs, fre, pnumb,fnumb)){
            pc[cand][fnumb] = pnumb;
            if(domerge && fnumb == 0 && canbe(fmins, fmaxs, fre, pnumb, fnumb+1)){ /* allow for f1,f2 merger */
                ncan++;
                pc[ncan][0] = pc[cand][0];
                ncan = candy(pc, fre, maxp, maxf, domerge, ncan, ncan,pnumb,fnumb+1,
                             fmins, fmaxs); /* same pole, next formant */
            }
            ncan = candy(pc, fre, maxp, maxf, domerge, ncan, cand,pnumb+1,fnumb+1,
                         fmins, fmaxs); /* next formant; next pole */
            if(((pnumb+1) < maxp) && canbe(fmins, fmaxs, fre, pnumb+1,fnumb)){
                /* try other frequencies for this formant */
                ncan++;			/* add one to the candidate index/tally */
                for(i=0; i<fnumb; i++)	/* clone the lower formants */
                    pc[ncan][i] = pc[cand][i];
                ncan = candy(pc, fre, maxp, maxf, domerge, ncan, ncan,pnumb+1,fnumb,
                             fmins, fmaxs);
            }
        } else {
            ncan = candy(pc, fre, maxp, maxf, domerge, ncan, cand,pnumb+1,fnumb,
                         fmins, fmaxs);
        }
    }
    /* If all pole frequencies have been examined without finding one which
       will map onto the current formant, go on to the next formant leaving the
       current formant null. */
    if((pnumb >= maxp) && (fnumb < maxf-1) && (pc[cand][fnumb] < 0)){
        if(fnumb){
            j=fnumb-1;
            while((j>0) && pc[cand][j] < 0) j--;
            i = ((j=pc[cand][j]) >= 0)? j : 0;
        } else i = 0;
        ncan = candy(pc, fre, maxp, maxf, domerge, ncan, cand,i,fnumb+1,
                     fmins, fmaxs);
    }

    return ncan;
}

/* Given a set of pole frequencies and allowable formant frequencies
   for nform formants, calculate all possible mappings of pole frequencies
   to formants, including, possibly, mappings with missing formants. */
/* freq: poles ordered by increasing FREQUENCY */
static int get_fcand(int npole, double *freq, int nform, short **pcan,
                     bool domerge, const double *fmins, const double *fmaxs)
{
    return candy(pcan, freq, npole, nform, domerge, 0, 0, 0, 0, fmins, fmaxs) + 1;
}

static void pole_dpform(pole_t *pole, const sound_t *ps, formants_t *f) {
    double minerr, dffact, ftemp, berr, ferr, bfact, ffact,
           rmsmax, fbias, *fr, rmsdffact, merger=0.0, merge_cost,
           FBIAS;
    int	ic, mincan=0;
    short	**pcan;
    int dmaxc,dminc,dcountf;
    bool domerge;

    double	fnom[]  = {  500, 1500, 2500, 3500, 4500, 5500, 6500},/*  "nominal" freqs.*/
                fmins[] = {   50,  400, 1000, 2000, 2000, 3000, 3000}, /* frequency bounds */
                fmaxs[] = { 1500, 3500, 4500, 5000, 6000, 6000, 8000}; /* for 1st 5 formants */

    if(NOM_FREQ > 0.0) {
        for(size_t i=0; i < MAX_FORMANTS; i++) {
            fnom[i] = ((i * 2) + 1) * NOM_FREQ;
            fmins[i] = fnom[i] - ((i+1) * NOM_FREQ) + 50.0;
            fmaxs[i] = fnom[i] + (i * NOM_FREQ) + 1000.0;
        }
    }
    rmsmax = pole->rms;
    FBIAS = F_BIAS /(.01 * ps->sample_count);
    /* Setup working values of the cost weights. */
    dffact = (DF_FACT * .01) * ps->sample_count; /* keep dffact scaled to frame rate */
    bfact = BAND_FACT /(.01 * ps->sample_count);
    ffact = DFN_FACT /(.01 * ps->sample_count);
    merge_cost = F_MERGE;
    if(merge_cost > 1000.0) domerge = false;

    /* Allocate space for the formant and bandwidth arrays to be passed back. */
    fr = malloc(sizeof(double) * FORMANT_COUNT);

    /* Allocate space for the raw candidate array. */
    pcan = malloc(sizeof(short*) * MAX_CANDIDATES);
    for(size_t i=0;i<MAX_CANDIDATES;i++)
        pcan[i] = malloc(sizeof(short) * FORMANT_COUNT);

    /* Allocate space for the dp lattice */
    dp_lattice_t fl;

    /* main formant tracking loop */
    size_t ncan = 0;		/* initialize candidate mapping count to 0 */

    /* moderate the cost of frequency jumps by the relative amplitude */
    rmsdffact = pole->rms;
    rmsdffact = rmsdffact/rmsmax;
    rmsdffact = rmsdffact * dffact;

    /* Get all likely mappings of the poles onto formants for this frame. */
    if(pole->npoles){	/* if there ARE pole frequencies available... */
        ncan = get_fcand(pole->npoles,pole->freq,FORMANT_COUNT,pcan, domerge,
                         fmins, fmaxs);

        /* Allocate space for this frame's candidates in the dp lattice. */
        fl.prept =  malloc(sizeof(short) * ncan);
        fl.cumerr = malloc(sizeof(double) * ncan);
        fl.cand =   malloc(sizeof(short*) * ncan);

        for(size_t j = 0; j < ncan; j++){	/* allocate cand. slots and install candidates */
            fl.cand[j] = malloc(sizeof(short) * FORMANT_COUNT);

            for(size_t k = 0; k < FORMANT_COUNT; k++)
                fl.cand[j][k] = pcan[j][k];
        }
    }
    fl.ncand = ncan;
    /* compute the distance between the current and previous mappings */
    for(size_t j = 0; j < ncan; j++) {	/* for each CURRENT mapping... */
            minerr = 0;

        fl.prept[j] = mincan; /* point to best previous mapping */
        /* (Note that mincan=-1 if there were no candidates in prev. fr.) */
        /* Compute the local costs for this current mapping. */
        berr = 0;
        ferr = 0;
        fbias = 0;
        for(size_t k = 0; k < FORMANT_COUNT; k++){
            ic = fl.cand[j][k];
            if(ic >= 0){
                if( !k ){		/* F1 candidate? */
                    ftemp = pole->freq[ic];
                    merger = (domerge &&
                            (ftemp == pole->freq[fl.cand[j][1]]))?
                        merge_cost: 0.0;
                }
                berr += pole->band[ic];
                ferr += (fabs(pole->freq[ic]-fnom[k])/fnom[k]);
                fbias += pole->freq[ic];
            } else {		/* if there was no freq. for this formant */
                fbias += fnom[k];
                berr += NOBAND;
                ferr += MISSING;
            }
        }

        /* Compute the total cost of this mapping and best previous. */
        fl.cumerr[j] = (FBIAS * fbias) + (bfact * berr) + merger +
            (ffact * ferr) + minerr;
    }			/* end for each CURRENT mapping... */

    /* Pick the candidate in the final frame with the lowest cost. */
    /* Starting with that min.-cost cand., work back thru the lattice. */
    dmaxc = 0;
    dminc = 100;
    dcountf = 0;
    mincan = -1;

    if (fl.ncand){	/* have candidates at this frame? */
        minerr = fl.cumerr[0];
        mincan = 0;
        for(size_t j=1; j<fl.ncand; j++)
            if( fl.cumerr[j] < minerr ){
                minerr = fl.cumerr[j];
                mincan = j;
            }
    }

    if(mincan >= 0){	/* if there is a "best" candidate at this frame */
        int j;
        if((j = fl.ncand) > dmaxc) dmaxc = j;
        else
            if( j < dminc) dminc = j;
        dcountf++;
        for(size_t j=0; j<FORMANT_COUNT; j++){
            int k = fl.cand[mincan][j];
            if(k >= 0){
                fr[j] = pole->freq[k];
            } else {		/* IF FORMANT IS MISSING... */
                fr[j] = fnom[j]; /* or insert neutral values */
            }
        }
        mincan = fl.prept[mincan];
    } else {		/* if no candidates, fake with "nominal" frequencies. */
        for(size_t j = 0; j < FORMANT_COUNT; j++){
            fr[j] = fnom[j];
        }
    }			/* note that mincan will remain =-1 if no candidates */
    /* Deallocate all the DP lattice work space. */
    if(fl.ncand){
        if(fl.cand) {
            for(size_t j = 0; j < fl.ncand; j++)
                free(fl.cand[j]);
            free(fl.cand);
            free(fl.cumerr);
            free(fl.prept);
        }
    }

    /* Deallocate space for the raw candidate aray. */
    for(size_t i=0;i<MAX_CANDIDATES;i++) free(pcan[i]);
    free(pcan);

    f->f1 = fr[0];
    f->f2 = fr[1];

    free(fr);
}

static void pole_lpc(pole_t *pole, const sound_t *sp) {
    double lpca[LPC_ORDER_MAX];
    double rr[LPC_ORDER_MAX], ri[LPC_ORDER_MAX];
    short *dporg;
    double flo;
    double x;

    x = M_PI / (LPC_ORDER + 1);

    dporg = malloc(sizeof(short) * sp->sample_count);

    for (size_t i = 0; i < sp->sample_count; i++)
        dporg[i] = (short) sound_get_sample(sp, 0, i);

    lpc(sp->sample_count, dporg, lpca, &pole->rms);

    /* set up starting points for the root search near unit circle */
    for (size_t i = 0; i <= LPC_ORDER; i += 1) {
        flo = LPC_ORDER - i;
        rr[i] = 2.0 * cos((flo + 0.5) * x);
        ri[i] = 2.0 * sin((flo + 0.5) * x);
    }

    /* don't waste time on low energy frames */
    if (pole->rms > 1.0) {
        formant(sp->sample_rate, lpca, &pole->npoles, pole->freq, pole->band,
            rr, ri);
    } else {			/* write out no pole frequencies */
        pole->npoles = 0;
    }

    free(dporg);
}

/*	Copyright (c) 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/* create the coefficients for a symmetric FIR lowpass filter using the
   window technique with a Hanning window. */
static void lc_lin_fir(double fc, int *nf, double *coef) {
    int	i, n;
    double	twopi, fn, c;

    if(((*nf % 2) != 1) || (*nf > 127)) {
        if(*nf <= 126) *nf = *nf + 1;
        else *nf = 127;
    }
    n = (*nf + 1)/2;

    /*  compute part of the ideal impulse response */
    twopi = M_PI * 2.0;
    coef[0] = 2.0 * fc;
    c = M_PI;
    fn = twopi * fc;
    for(i=1;i < n; i++) coef[i] = sin(i * fn)/(c * i);

    /* Now apply a Hanning window to the (infinite) impulse response. */
    fn = twopi/((double)(*nf - 1));
    for(i=0;i<n;i++)
        coef[i] *= (.5 + (.5 * cos(fn * ((double)i))));
}

/* ic contains 1/2 the coefficients of a symmetric FIR filter with unity
   passband gain.  This filter is convolved with the signal in buf.
   The output is placed in buf2.  If invert != 0, the filter magnitude
   response will be inverted. */
static void do_fir(short *buf, int in_samps, short *bufo, int ncoef,
                   short *ic, int invert)
{
    short  *buft, *bufp, *bufp2, stem;
    short co[256], mem[256];
    int i, j, k, l, m, sum, integral;

    bufp = ic + ncoef - 1;
    bufp2 = co;
    buft = co + ((ncoef - 1) * 2);
    integral = 0;

    for (i = ncoef - 1; i > 0; i -= 1) {
        if (!invert) {
            *buft = *bufp2 = *bufp;

            buft -= 1;
            bufp2 += 1;
            bufp -= 1;
        } else {
            integral += (stem = *bufp);
            *buft = *bufp2 = -stem;

            bufp -= 1;
            buft -= 1;
            bufp2 += 1;
        }
    }

    if (!invert) {
        *buft = *bufp2 = *bufp; /* point of symmetry */
    } else {
        integral *= 2;
        integral += *bufp;
        *buft = integral - *bufp;
    }

    buft = mem;

    for (i = ncoef - 1; i > 0; i -= 1)
        *buft++ = 0;

    for (i = ncoef; i > 0; i -= 1)
        *buft++ = *buf++;

    l = 16384;
    m = 15;
    k = (ncoef << 1) - 1;

    for (i = in_samps - ncoef; i > 0; i -= 1) {
        buft = mem;
        bufp = co;
        bufp2 = mem + 1;
        sum = 0;

        for (j = k; j > 0; j -= 1) {
            sum += (*bufp * *buft + l) >> m;
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

    for (i = ncoef; i > 0; i -= 1) {	/* pad data end with zeros */
        buft = mem;
        bufp = co;
        bufp2 = mem + 1;
        sum = 0;

        for (j = k; j > 0; j -= 1) {
            sum += (*bufp * *buft + l) >> m;
            *buft = *bufp2;

            bufp += 1;
            buft += 1;
            bufp2 += 1;
        }
        *--buft = 0;
        *bufo++ = sum;
    }
}

static int get_abs_maximum(short *d, int n) {
    int i;
    short amax, t;

    if((t = *d++) >= 0) amax = t;
    else amax = -t;

    for(i = n-1; i-- > 0; ) {
        if((t = *d++) > amax) amax = t;
        else {
            if(-t > amax) amax = -t;
        }
    }
    return((int)amax);
}

static void dwnsamp(short *buf, int in_samps, short **buf2, size_t *out_samps,
                    int insert, int decimate, int ncoef, short *ic, int *smin,
                    int *smax)
{
    short  *bufp, *bufp2;
    short	*buft;
    int i, j, k, l, m;
    int imax, imin;

    *buf2 = buft = malloc(sizeof(short)*insert*in_samps);

    k = imax = get_abs_maximum(buf,in_samps);
    if (k == 0) k = 1;
    if(insert > 1) k = (32767 * 32767)/k;	/*  prepare to scale data */
    else k = (16384 * 32767)/k;
    l = 16384;
    m = 15;

    /* Insert zero samples to boost the sampling frequency and scale the
       signal to maintain maximum precision. */
    for(i=0, bufp=buft, bufp2=buf; i < in_samps; i++) {
        *bufp++ = ((k * (*bufp2++)) + l) >> m ;
        for(j=1; j < insert; j++) *bufp++ = 0;
    }

    do_fir(buft,in_samps*insert,buft,ncoef,ic,0);

    /*	Finally, decimate and return the downsampled signal. */
    *out_samps = j = (in_samps * insert)/decimate;
    k = decimate;
    for(i=0, bufp=buft, imax = imin = *bufp; i < j; bufp += k,i++) {
        *buft++ = *bufp;
        if(imax < *bufp) imax = *bufp;
        else
            if(imin > *bufp) imin = *bufp;
    }
    *smin = imin;
    *smax = imax;
    *buf2 = realloc(*buf2,sizeof(short) * (*out_samps));
}

static int ratprx(double a, int *k, int *l, int qlim) {
    double aa, af, q, em, qq = 0, pp = 0, ps, e;
    int	ai, ip, i;

    aa = fabs(a);
    ai = (int) aa;
    i = (int) aa;
    af = aa - i;
    q = 0;
    em = 1.0;
    while(++q <= qlim) {
        ps = q * af;
        ip = (int) (ps + 0.5);
        e = fabs((ps - (double)ip)/q);
        if(e < em) {
            em = e;
            pp = ip;
            qq = q;
        }
    };
    *k = (int) ((ai * qq) + pp);
    *k = (a > 0)? *k : -(*k);
    *l = (int) qq;
    return(true);
}

void sound_downsample(sound_t *s, size_t freq2) {
    enum { N_BITS = 15 };

    short	*bufin, **bufout;
    double	beta = 0.0, b[256];
    double	tratio, maxi, ratio, beta_new, freq1;
    int	ncoeff = 127;
    short	ic[256];
    int	insert, decimate, smin, smax;

    size_t out_samps;
    size_t j;
    size_t ncoefft;

    freq1 = s->sample_rate;
    ratio = freq2/freq1;
    ratprx(ratio,&insert,&decimate,10);
    tratio = ((double)insert)/((double)decimate);

    if(tratio > .99)
        return;

    ncoefft = 0;

    bufout = malloc(sizeof(short*));
    bufin = malloc(sizeof(short) * s->sample_count);

    for (size_t i = 0; i < s->sample_count; i++) {
        bufin[i] = (short) sound_get_sample(s, 0, i);
    }

    freq2 = tratio * freq1;
    beta_new = (.5 * freq2)/(insert * freq1);

    if(beta != beta_new){
        beta = beta_new;
        lc_lin_fir(beta,&ncoeff,b);
        maxi = (1 << N_BITS) - 1;
        j = (ncoeff/2) + 1;
        for(size_t i = 0; i < j; i++){
            ic[i] = (int) (0.5 + (maxi * b[i]));
            if(ic[i]) ncoefft = i+1;
        }
    }				/*  endif new coefficients need to be computed */

    dwnsamp(bufin, s->sample_count, bufout, &out_samps, insert, decimate, ncoefft, ic,
            &smin, &smax);

    for (size_t i = 0; i < out_samps; i++) {
        sound_set_sample(s, 0, i, (*bufout)[i]);
    }

    s->sample_count = out_samps;
    s->sample_rate = (int)freq2;

    free(*bufout);
    free(bufout);
    free(bufin);
}

void formants_calc(formants_t *f, const sound_t *s) {
    pole_t pole;

    pole_lpc(&pole, s);
    pole_dpform(&pole, s, f);
}

#ifdef LIBFORMANT_TEST
SUITE(formant_suite) {
    RUN_TEST(test_sound_load_samples);
}
#endif
