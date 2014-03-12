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
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "formant.h"
#include "processing.h"

#ifdef LIBFORMANT_TEST
#include "greatest.h"
#endif

enum { MAX_FORMANTS = 7 };

enum { LPC_ORDER_MIN = 2 };
enum { LPC_ORDER_MAX = 30 };

#define PI 3.14159265358979323846

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
    double *cumerr; 	 /* cum. errors associated with each cand. */
} form_t;

typedef struct {   /* structure to hold raw LPC analysis data */
    double rms;    /* rms for current LPC analysis frame */
    double rms2;    /* rms for current F0 analysis frame */
    double f0;     /* fundamental frequency estimate for this frame */
    double pv;		/* probability that frame is voiced */
    double change; /* spec. distance between current and prev. frames */
    size_t npoles; /* # of complex poles from roots of LPC polynomial */
    double *freq;  /* array of complex pole frequencies (Hz) */
    double *band;  /* array of complex pole bandwidths (Hz) */
} pole_t;

void formant_opts_init(formant_opts_t *opts) {
    *opts = (formant_opts_t) {
        .n_formants = 4,

        .downsample_rate = 10000,
        .pre_emph_factor = 0.7,

        .window_type = WINDOW_TYPE_RECTANGULAR,
        .window_dur = 0.049,
        .frame_dur = 0.01,

        .lpc_type = LPC_TYPE_NORMAL,
        .lpc_order = 12,
        .nom_freq = -10,
    };
}

bool formant_opts_process(formant_opts_t *opts) {
    if (opts->lpc_order > LPC_ORDER_MAX || opts->lpc_order < LPC_ORDER_MIN)
        return false;

    if (opts->n_formants > (opts->lpc_order - 4) / 2)
        return false;

    if (opts->n_formants > MAX_FORMANTS)
        return false;

    /* force "standard" stabilized covariance (ala bsa) */
    if (opts->lpc_type == LPC_TYPE_BSA) {
        opts->window_dur = 0.025;
        /* exp(-1800*pi*T) */
        opts->pre_emph_factor = exp(-62.831853 * 90 / opts->downsample_rate);
    }

    return true;
}

#ifdef LIBFORMANT_TEST
TEST test_formant_opts_process() {
    formant_opts_t opts;
    formant_opts_init(&opts);

    GREATEST_ASSERTm("default options should validate",
        formant_opts_process(&opts));

    PASS();
}
#endif

void sound_init(sound_t *s) {
    *s = (sound_t) {
        .sample_rate = 0,
        .n_channels = 0,
        .n_samples = 0,
        .samples = NULL,
    };
}

void sound_reset(sound_t *s, size_t sample_rate, size_t n_channels) {
    s->sample_rate = sample_rate;
    s->n_channels = n_channels;
}

void sound_destroy(sound_t *s) {
    free(s->samples);
}

void sound_resize(sound_t *s, size_t n_samples) {
    if (n_samples > s->n_samples * s->n_channels)
        s->samples = realloc(s->samples, n_samples * sizeof(formant_sample_t));

    // The rest of the processing functions expect n_samples to be the number of
    // samples per channel.
    s->n_samples = n_samples / s->n_channels;
}

void sound_load_samples(sound_t *s, const formant_sample_t *samples, size_t n_samples) {
    sound_resize(s, n_samples);
    memcpy(s->samples, samples, n_samples * sizeof(formant_sample_t));
}

#ifdef LIBFORMANT_TEST
TEST test_sound_load_samples() {
    sound_t s;
    sound_init(&s);
    sound_reset(&s, 44100, 2);

    sound_load_samples(&s, NULL, 0);
    GREATEST_ASSERT_EQm("don't allocate with no samples", s.samples, NULL);
    GREATEST_ASSERT_EQm("don't set n_samples with no samples", s.n_samples, 0);

    formant_sample_t samples[8] = {0, 0, 0, 0, 0, 0, 0, 42};
    sound_load_samples(&s, samples, 8);
    GREATEST_ASSERT_EQm("set n_samples correctly", s.n_samples, 4);
    GREATEST_ASSERTm("samples allocated", s.samples);
    GREATEST_ASSERT_EQm("copy samples correctly", s.samples[7], 42);

    PASS();
}
#endif

static inline void sound_set_sample(sound_t *s, size_t chan, size_t i,
                                    formant_sample_t val)
{
    s->samples[i * s->n_channels + chan] = val;
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

/* find the maximum in the "stationarity" function (stored in rms) */
static double get_stat_max(pole_t **poles, int nframes) {
    int i;
    double amax, t;

    for(i=1, amax = (*poles++)->rms; i++ < nframes; )
        if((t = (*poles++)->rms) > amax) amax = t;

    return(amax);
}

static void dpform(sound_t *ps, pole_t **poles, size_t nform, double nom_f1) {
    double pferr, conerr, minerr, dffact, ftemp, berr, ferr, bfact, ffact,
           rmsmax, fbias, **fr, **ba, rmsdffact, merger=0.0, merge_cost,
           FBIAS;
    int	ic, ip, mincan=0;
    short	**pcan;
    form_t	**fl;
    int dmaxc,dminc,dcountf;
    bool domerge;

    double	fnom[]  = {  500, 1500, 2500, 3500, 4500, 5500, 6500},/*  "nominal" freqs.*/
                fmins[] = {   50,  400, 1000, 2000, 2000, 3000, 3000}, /* frequency bounds */
                fmaxs[] = { 1500, 3500, 4500, 5000, 6000, 6000, 8000}; /* for 1st 5 formants */

    if(nom_f1 > 0.0) {
        for(size_t i=0; i < MAX_FORMANTS; i++) {
            fnom[i] = ((i * 2) + 1) * nom_f1;
            fmins[i] = fnom[i] - ((i+1) * nom_f1) + 50.0;
            fmaxs[i] = fnom[i] + (i * nom_f1) + 1000.0;
        }
    }
    rmsmax = get_stat_max(poles, ps->n_samples);
    FBIAS = F_BIAS /(.01 * ps->sample_rate);
    /* Setup working values of the cost weights. */
    dffact = (DF_FACT * .01) * ps->sample_rate; /* keep dffact scaled to frame rate */
    bfact = BAND_FACT /(.01 * ps->sample_rate);
    ffact = DFN_FACT /(.01 * ps->sample_rate);
    merge_cost = F_MERGE;
    if(merge_cost > 1000.0) domerge = false;

    /* Allocate space for the formant and bandwidth arrays to be passed back. */
    fr = malloc(sizeof(double*) * nform * 2);
    ba = fr + nform;
    for(size_t i=0;i < nform*2; i++){
        fr[i] = malloc(sizeof(double) * ps->n_samples);
    }

    /* Allocate space for the raw candidate array. */
    pcan = malloc(sizeof(short*) * MAX_CANDIDATES);
    for(size_t i=0;i<MAX_CANDIDATES;i++)
        pcan[i] = malloc(sizeof(short) * nform);

    /* Allocate space for the dp lattice */
    fl = malloc(sizeof(form_t*) * ps->n_samples);
    for(size_t i=0;i < ps->n_samples; i++)
        fl[i] = malloc(sizeof(form_t));

    /* main formant tracking loop */
    for(size_t i = 0; i < ps->n_samples; i++) {	/* for all analysis frames... */
        size_t ncan = 0;		/* initialize candidate mapping count to 0 */

        /* moderate the cost of frequency jumps by the relative amplitude */
        rmsdffact = poles[i]->rms;
        rmsdffact = rmsdffact/rmsmax;
        rmsdffact = rmsdffact * dffact;

        /* Get all likely mappings of the poles onto formants for this frame. */
        if(poles[i]->npoles){	/* if there ARE pole frequencies available... */
            ncan = get_fcand(poles[i]->npoles,poles[i]->freq,nform,pcan, domerge,
                             fmins, fmaxs);

            /* Allocate space for this frame's candidates in the dp lattice. */
            fl[i]->prept =  malloc(sizeof(short) * ncan);
            fl[i]->cumerr = malloc(sizeof(double) * ncan);
            fl[i]->cand =   malloc(sizeof(short*) * ncan);

            for(size_t j = 0; j < ncan; j++){	/* allocate cand. slots and install candidates */
                fl[i]->cand[j] = malloc(sizeof(short) * nform);

                for(size_t k = 0; k < nform; k++)
                    fl[i]->cand[j][k] = pcan[j][k];
            }
        }
        fl[i]->ncand = ncan;
        /* compute the distance between the current and previous mappings */
        for(size_t j = 0; j < ncan; j++) {	/* for each CURRENT mapping... */
            if( i ){		/* past the first frame? */
                minerr = 0;
                if(fl[i-1]->ncand) minerr = 2.0e30;
                mincan = -1;
                for(size_t k = 0; k < fl[i-1]->ncand; k++){ /* for each PREVIOUS map... */
                    pferr = 0.0;
                    for(size_t l = 0; l < nform; l++){
                        ic = fl[i]->cand[j][l];
                        ip = fl[i-1]->cand[k][l];
                        if((ic >= 0)	&& (ip >= 0)){
                            ftemp = 2.0 * fabs(poles[i]->freq[ic] - poles[i-1]->freq[ip])/
                                (poles[i]->freq[ic] + poles[i-1]->freq[ip]);
                            /* cost prop. to SQUARE of deviation to discourage large jumps */
                            pferr += ftemp * ftemp;
                        }
                        else pferr += MISSING;
                    }
                    /* scale delta-frequency cost and add in prev. cum. cost */
                    conerr = (rmsdffact * pferr) + fl[i-1]->cumerr[k];
                    if(conerr < minerr){
                        minerr = conerr;
                        mincan = k;
                    }
                }			/* end for each PREVIOUS mapping... */
            }	else {		/* (i.e. if this is the first frame... ) */
                minerr = 0;
            }

            fl[i]->prept[j] = mincan; /* point to best previous mapping */
            /* (Note that mincan=-1 if there were no candidates in prev. fr.) */
            /* Compute the local costs for this current mapping. */
            berr = 0;
            ferr = 0;
            fbias = 0;
            for(size_t k = 0; k < nform; k++){
                ic = fl[i]->cand[j][k];
                if(ic >= 0){
                    if( !k ){		/* F1 candidate? */
                        ftemp = poles[i]->freq[ic];
                        merger = (domerge &&
                                (ftemp == poles[i]->freq[fl[i]->cand[j][1]]))?
                            merge_cost: 0.0;
                    }
                    berr += poles[i]->band[ic];
                    ferr += (fabs(poles[i]->freq[ic]-fnom[k])/fnom[k]);
                    fbias += poles[i]->freq[ic];
                } else {		/* if there was no freq. for this formant */
                    fbias += fnom[k];
                    berr += NOBAND;
                    ferr += MISSING;
                }
            }

            /* Compute the total cost of this mapping and best previous. */
            fl[i]->cumerr[j] = (FBIAS * fbias) + (bfact * berr) + merger +
                (ffact * ferr) + minerr;
        }			/* end for each CURRENT mapping... */
    }				/* end for all analysis frames... */

    /* Pick the candidate in the final frame with the lowest cost. */
    /* Starting with that min.-cost cand., work back thru the lattice. */
    dmaxc = 0;
    dminc = 100;
    dcountf = 0;
    mincan = -1;
    for (size_t m = 1; m <= ps->n_samples; m += 1) {
        size_t i = ps->n_samples - m;
        if(mincan < 0)		/* need to find best starting candidate? */
            if(fl[i]->ncand){	/* have candidates at this frame? */
                minerr = fl[i]->cumerr[0];
                mincan = 0;
                for(size_t j=1; j<fl[i]->ncand; j++)
                    if( fl[i]->cumerr[j] < minerr ){
                        minerr = fl[i]->cumerr[j];
                        mincan = j;
                    }
            }
        if(mincan >= 0){	/* if there is a "best" candidate at this frame */
            int j;
            if((j = fl[i]->ncand) > dmaxc) dmaxc = j;
            else
                if( j < dminc) dminc = j;
            dcountf++;
            for(size_t j=0; j<nform; j++){
                int k = fl[i]->cand[mincan][j];
                if(k >= 0){
                    fr[j][i] = poles[i]->freq[k];
                    ba[j][i] = poles[i]->band[k];
                } else {		/* IF FORMANT IS MISSING... */
                    if(i < ps->n_samples - 1){
                        fr[j][i] = fr[j][i+1]; /* replicate backwards */
                        ba[j][i] = ba[j][i+1];
                    } else {
                        fr[j][i] = fnom[j]; /* or insert neutral values */
                        ba[j][i] = NOBAND;
                    }
                }
            }
            mincan = fl[i]->prept[mincan];
        } else {		/* if no candidates, fake with "nominal" frequencies. */
            for(size_t j = 0; j < nform; j++){
                fr[j][i] = fnom[j];
                ba[j][i] = NOBAND;
            }
        }			/* note that mincan will remain =-1 if no candidates */
    }				/* end unpacking formant tracks from the dp lattice */
    /* Deallocate all the DP lattice work space. */
    for (size_t m = 1; m <= ps->n_samples; m += 1) {
        size_t i = ps->n_samples - m;
        if(fl[i]->ncand){
            if(fl[i]->cand) {
                for(size_t j = 0; j < fl[i]->ncand; j++)
                    free(fl[i]->cand[j]);
                free(fl[i]->cand);
                free(fl[i]->cumerr);
                free(fl[i]->prept);
            }
        }
    }
    for(size_t i=0; i<ps->n_samples; i++)
        free(fl[i]);
    free(fl);
    fl = 0;

    for(size_t i=0; i<ps->n_samples; i++) {
        free(poles[i]->freq);
        free(poles[i]->band);
        free(poles[i]);
    }
    free(poles);

    /* Deallocate space for the raw candidate aray. */
    for(size_t i=0;i<MAX_CANDIDATES;i++) free(pcan[i]);
    free(pcan);

    ps->n_channels = nform * 2;

    for (size_t i = 0; i < ps->n_samples; i++)
        for (size_t j = 0; j < ps->n_channels; j++)
            sound_set_sample(ps, j, i, fr[j][i]);

    for(size_t i = 0; i < nform*2; i++) free(fr[i]);
    free(fr);
}

/* computation and I/O routines for dealing with LPC poles */
static double frand() {
    return (((double)rand())/(double)RAND_MAX);
}

/* a quick and dirty interface to bsa's stabilized covariance LPC */
static int lpcbsa(int np, int wind, short *data, double *lpc, double *energy,
                  double preemp)
{
    int i, owind=0, wind1;
    double w[1000];
    double rc[LPC_ORDER_MAX],phi[LPC_ORDER_MAX*LPC_ORDER_MAX],shi[LPC_ORDER_MAX],sig[1000];
    double xl = .09, fham, amax;
    double *psp1, *psp3, *pspl;

    if(owind != wind) {		/* need to compute a new window? */
        fham = 6.28318506 / wind;
        for(psp1=w,i=0;i<wind;i++,psp1++)
            *psp1 = .54 - .46 * cos(i * fham);
        owind = wind;
    }
    wind += np + 1;
    wind1 = wind-1;

    for(psp3=sig,pspl=sig+wind; psp3 < pspl; )
        *psp3++ = (double)(*data++) + .016 * frand() - .008;
    for(psp3=sig+1,pspl=sig+wind;psp3<pspl;psp3++)
        *(psp3-1) = *psp3 - preemp * *(psp3-1);
    for(amax = 0.,psp3=sig+np,pspl=sig+wind1;psp3<pspl;psp3++)
        amax += *psp3 * *psp3;
    *energy = sqrt(amax / (double)owind);
    amax = 1.0/(*energy);

    for(psp3=sig,pspl=sig+wind1;psp3<pspl;psp3++)
        *psp3 *= amax;

    return dlpcwtd(sig,&wind1,lpc,&np,rc,phi,shi,&xl,w) == np;
}

static pole_t **lpc_poles(sound_t *sp, const formant_opts_t *opts) {
    enum { LPC_STABLE = 70 };

    int size, step, nform, init;
    size_t nfrm;
    pole_t **poles;
    double energy, lpca[LPC_ORDER_MAX], normerr, *bap, *frp, *rhp;
    double rr[LPC_ORDER_MAX], ri[LPC_ORDER_MAX];
    short *datap, *dporg;
    int ord;
    double alpha, r0;
    double flo;
    double x;

    // Duration of the given samples in seconds.
    double samples_dur;

    bap = NULL;
    frp = NULL;
    rhp = NULL;

    x = PI / (opts->lpc_order + 1);

    samples_dur = (double)(sp->n_samples) / sp->sample_rate;

    if (samples_dur < opts->window_dur)
        return NULL;

    nfrm = 1 + (int)((samples_dur - opts->window_dur) / opts->frame_dur);
    size = (int)(.5 + opts->window_dur * sp->sample_rate);
    step = (int)(.5 + opts->frame_dur * sp->sample_rate);
    poles = malloc(nfrm * sizeof(pole_t *));
    dporg = malloc(sizeof(short) * sp->n_samples);
    datap = dporg;

    for (size_t i = 0; i < sp->n_samples; i++)
        datap[i] = (short) sound_get_sample(sp, 0, i);

    init = true;

    for (size_t j = 0; j < nfrm; j += 1) {
        poles[j] = malloc(sizeof(pole_t));
        poles[j]->freq = frp = malloc(sizeof(double)*opts->lpc_order);
        poles[j]->band = bap = malloc(sizeof(double)*opts->lpc_order);

        switch(opts->lpc_type) {
        case LPC_TYPE_NORMAL:
            lpc(opts->lpc_order, LPC_STABLE, size, datap, lpca, rhp, NULL, &normerr,
                &energy, opts->pre_emph_factor, opts->window_type);
        break;

        case LPC_TYPE_BSA:
            lpcbsa(opts->lpc_order, size, datap, lpca, &energy, opts->pre_emph_factor);
        break;

        case LPC_TYPE_COVAR:
            ord = opts->lpc_order;
            w_covar(datap, &ord, size, 0, lpca, &alpha, &r0, opts->pre_emph_factor, 0);
            energy = sqrt(r0 / (size - ord));
        break;

        case LPC_TYPE_INVALID:
        break;
        }
        poles[j]->change = 0.0;

        /* set up starting points for the root search near unit circle */
        if (init) {
            for (size_t i = 0; i <= opts->lpc_order; i += 1) {
                flo = opts->lpc_order - i;
                rr[i] = 2.0 * cos((flo + 0.5) * x);
                ri[i] = 2.0 * sin((flo + 0.5) * x);
            }
        }

        poles[j]->rms = energy;

        /* don't waste time on low energy frames */
        if (energy > 1.0) {
            formant(opts->lpc_order, sp->sample_rate, lpca, &nform, frp, bap, rr, ri);
            poles[j]->npoles = nform;
            init=false;		/* use old poles to start next search */
        } else {			/* write out no pole frequencies */
            poles[j]->npoles = 0;
            init = true;		/* restart root search in a neutral zone */
        }

        datap += step;
    }

    free(dporg);

    sp->sample_rate = (size_t)(1.0 / opts->frame_dur);
    sp->n_channels = opts->lpc_order;
    sp->n_samples = nfrm;

    for (size_t i = 0; i < nfrm; i++)
        for (size_t j = 0; j < opts->lpc_order; j++)
            sound_set_sample(sp, j, i, poles[i]->freq[j]);

    return poles;
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
    twopi = PI * 2.0;
    coef[0] = 2.0 * fc;
    c = PI;
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
    /*    af = fmod(aa,1.0); */
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

static void Fdownsample(sound_t *s, double freq2) {
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
    bufin = malloc(sizeof(short) * s->n_samples);

    for (size_t i = 0; i < s->n_samples; i++) {
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

    dwnsamp(bufin, s->n_samples, bufout, &out_samps, insert, decimate, ncoefft, ic,
            &smin, &smax);

    for (size_t i = 0; i < out_samps; i++) {
        sound_set_sample(s, 0, i, (*bufout)[i]);
    }

    s->n_samples = out_samps;
    s->sample_rate = (int)freq2;

    free(*bufout);
    free(bufout);
    free(bufin);
}

static void highpass(sound_t *s) {
    /* This assumes the sampling frequency is 10kHz and that the FIR
       is a Hanning function of (LCSIZ/10)ms duration. */
    enum { LCSIZ = 101 };

    short *datain, *dataout;
    short *lcf;
    size_t len;
    double scale, fn;

    datain = malloc(sizeof(short) * s->n_samples);
    dataout = malloc(sizeof(short) * s->n_samples);
    for (size_t i = 0; i < s->n_samples; i++) {
        datain[i] = (short) sound_get_sample(s, 0, i);
    }

    lcf = malloc(sizeof(short) * LCSIZ);
    len = 1 + (LCSIZ/2);
    fn = PI * 2.0 / (LCSIZ - 1);
    scale = 32767.0/(.5 * LCSIZ);
    for(size_t i=0; i < len; i++)
        lcf[i] = (short) (scale * (.5 + (.4 * cos(fn * ((double)i)))));

    do_fir(datain,s->n_samples,dataout,len,lcf,1); /* in downsample.c */

    for (size_t i = 0; i < s->n_samples; i++) {
        sound_set_sample(s, 0, i, dataout[i]);
    }

    free(lcf);
    free(dataout);
    free(datain);
}

bool sound_calc_formants(sound_t *s, const formant_opts_t *opts) {
    pole_t **poles;

    if (opts->downsample_rate < s->sample_rate)
        Fdownsample(s, opts->downsample_rate);

    /* be sure DC and rumble are gone! */
    if (opts->pre_emph_factor < 1.0)
        highpass(s);

    poles = lpc_poles(s, opts);

    if (!poles)
        return false;

    dpform(s, poles, opts->n_formants, opts->nom_freq);

    return true;
}

#ifdef LIBFORMANT_TEST
SUITE(formant_suite) {
    RUN_TEST(test_formant_opts_process);
    RUN_TEST(test_sound_load_samples);
}
#endif
