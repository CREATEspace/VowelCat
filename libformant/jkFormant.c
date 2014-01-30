/* formant.c */
/*
 * This software has been licensed to the Centre of Speech Technology, KTH
 * by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
 * file BSD.txt, which is a BSD style license.
 *
 *    "Copyright (c) 1987-1990  AT&T, Inc.
 *    "Copyright (c) 1986-1990  Entropic Speech, Inc.
 *    "Copyright (c) 1990-1994  Entropic Research Laboratory, Inc.
 *                   All rights reserved"
 *
 * Written by:  David Talkin
 * Revised by: John Shore
 *
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jkFormant.h"
#include "sigproc2.h"

#define MAXFORMANTS 7

static inline void Snack_SetSample(sound_t *s, size_t chan, size_t i,
                                   storage_t val)
{
    s->blocks[i * s->nchannels + chan] = val;
}

static inline storage_t Snack_GetSample(sound_t *s, size_t chan, size_t i) {
    return s->blocks[i * s->nchannels + chan];
}

sound_t *Snack_NewSound(int rate, int nchannels) {
    sound_t *s = malloc(sizeof(sound_t));

    if (s == NULL) {
        return NULL;
    }

    /* Default sound specifications */

    s->samprate = rate;
    s->nchannels = nchannels;
    s->length = 0;
    s->blocks = NULL;
    s->pole = NULL;

    return s;
}

void Snack_ResizeSoundStorage(sound_t *s, int len) {
    free(s->blocks);

    s->length = len;
    s->blocks = malloc(len * sizeof(storage_t) * s->nchannels);
}

void Snack_DeleteSound(sound_t *s) {
    free(s->blocks);
    free(s);
}

void LoadSound(sound_t *s, short *samples, size_t len) {
    Snack_ResizeSoundStorage(s, len);

    for (size_t i = 0; i < s->length * s->nchannels; i += 1)
        s->blocks[i] = (storage_t) samples[i];
}


/*	dpform.c       */

/* a formant tracker based on LPC polynomial roots and dynamic programming */
/***/
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

/* Here are the major fudge factors for tweaking the formant tracker. */
#define MAXCAN	300  /* maximum number of candidate mappings allowed */
static const double MISSING = 1, /* equivalent delta-Hz cost for missing formant */
              NOBAND = 1000, /* equivalent bandwidth cost of a missing formant */
              DF_FACT =  20.0, /* cost for proportional frequency changes */
              /* with good "stationarity" function:*/
              /*        DF_FACT =  80.0, *//*  cost for proportional frequency changes */
              DFN_FACT = 0.3, /* cost for proportional dev. from nominal freqs. */
              BAND_FACT = .002, /* cost per Hz of bandwidth in the poles */
              /*	F_BIAS	  = 0.0004,   bias toward selecting low-freq. poles */
              F_BIAS	  = 0.000, /*  bias toward selecting low-freq. poles */
              F_MERGE = 2000.0; /* cost of mapping f1 and f2 to same frequency */

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
        /*   printf("\ncan:%3d  pnumb:%3d  fnumb:%3d",cand,pnumb,fnumb); */
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
                /*		printf("\n%4d  %4d  %4d",ncan,pnumb+1,fnumb); */
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

/*      ----------------------------------------------------------      */
/* find the maximum in the "stationarity" function (stored in rms) */
static double get_stat_max(pole_t **pole, int nframes) {
    int i;
    double amax, t;

    for(i=1, amax = (*pole++)->rms; i++ < nframes; )
        if((t = (*pole++)->rms) > amax) amax = t;

    return(amax);
}

static sound_t *dpform(sound_t *ps, int nform, double nom_f1) {
    double pferr, conerr, minerr, dffact, ftemp, berr, ferr, bfact, ffact,
           rmsmax, fbias, **fr, **ba, rmsdffact, merger=0.0, merge_cost,
           FBIAS;
    int	i, j, k, l, ic, ip, mincan=0;
    short	**pcan;
    form_t	**fl;
    pole_t	**pole; /* raw LPC pole data structure array */
    sound_t *fbs;
    int dmaxc,dminc,dcountc,dcountf;
    bool domerge;

    double	fnom[]  = {  500, 1500, 2500, 3500, 4500, 5500, 6500},/*  "nominal" freqs.*/
                fmins[] = {   50,  400, 1000, 2000, 2000, 3000, 3000}, /* frequency bounds */
                fmaxs[] = { 1500, 3500, 4500, 5000, 6000, 6000, 8000}; /* for 1st 5 formants */

    if(ps) {
        if(nom_f1 > 0.0) {
            for(int i=0; i < MAXFORMANTS; i++) {
                fnom[i] = ((i * 2) + 1) * nom_f1;
                fmins[i] = fnom[i] - ((i+1) * nom_f1) + 50.0;
                fmaxs[i] = fnom[i] + (i * nom_f1) + 1000.0;
            }
        }
        pole = ps->pole;
        rmsmax = get_stat_max(pole, ps->length);
        FBIAS = F_BIAS /(.01 * ps->samprate);
        /* Setup working values of the cost weights. */
        dffact = (DF_FACT * .01) * ps->samprate; /* keep dffact scaled to frame rate */
        bfact = BAND_FACT /(.01 * ps->samprate);
        ffact = DFN_FACT /(.01 * ps->samprate);
        merge_cost = F_MERGE;
        if(merge_cost > 1000.0) domerge = false;

        /* Allocate space for the formant and bandwidth arrays to be passed back. */
        fr = malloc(sizeof(double*) * nform * 2);
        ba = fr + nform;
        for(i=0;i < nform*2; i++){
            fr[i] = malloc(sizeof(double) * ps->length);
        }
        /*    cp = new_ext(ps->name,"fb");*/
        /*    if((fbs=new_signal(cp,SIG_UNKNOWN,dup_header(ps->header),fr,ps->length,		       ps->samprate, nform * 2))) {*/
        if (1) {
            /* Allocate space for the raw candidate array. */
            pcan = malloc(sizeof(short*) * MAXCAN);
            for(i=0;i<MAXCAN;i++) pcan[i] = malloc(sizeof(short) * nform);

            /* Allocate space for the dp lattice */
            fl = malloc(sizeof(form_t*) * ps->length);
            for(i=0;i < ps->length; i++)
                fl[i] = malloc(sizeof(form_t));

            /*******************************************************************/
            /* main formant tracking loop */
            /*******************************************************************/
            for(i=0; i < ps->length; i++){	/* for all analysis frames... */

                int ncan = 0;		/* initialize candidate mapping count to 0 */

                /* moderate the cost of frequency jumps by the relative amplitude */
                rmsdffact = pole[i]->rms;
                rmsdffact = rmsdffact/rmsmax;
                rmsdffact = rmsdffact * dffact;

                /* Get all likely mappings of the poles onto formants for this frame. */
                if(pole[i]->npoles){	/* if there ARE pole frequencies available... */
                    ncan = get_fcand(pole[i]->npoles,pole[i]->freq,nform,pcan, domerge,
                                     fmins, fmaxs);

                    /* Allocate space for this frame's candidates in the dp lattice. */
                    fl[i]->prept =  malloc(sizeof(short) * ncan);
                    fl[i]->cumerr = malloc(sizeof(double) * ncan);
                    fl[i]->cand =   malloc(sizeof(short*) * ncan);
                    for(j=0;j<ncan;j++){	/* allocate cand. slots and install candidates */
                        fl[i]->cand[j] = malloc(sizeof(short) * nform);
                        for(k=0; k<nform; k++)
                            fl[i]->cand[j][k] = pcan[j][k];
                    }
                }
                fl[i]->ncand = ncan;
                /* compute the distance between the current and previous mappings */
                for(j=0;j<ncan;j++){	/* for each CURRENT mapping... */
                    if( i ){		/* past the first frame? */
                        minerr = 0;
                        if(fl[i-1]->ncand) minerr = 2.0e30;
                        mincan = -1;
                        for(k=0; k < fl[i-1]->ncand; k++){ /* for each PREVIOUS map... */
                            for(pferr=0.0, l=0; l<nform; l++){
                                ic = fl[i]->cand[j][l];
                                ip = fl[i-1]->cand[k][l];
                                if((ic >= 0)	&& (ip >= 0)){
                                    ftemp = 2.0 * fabs(pole[i]->freq[ic] - pole[i-1]->freq[ip])/
                                        (pole[i]->freq[ic] + pole[i-1]->freq[ip]);
                                    /*		  ftemp = pole[i]->freq[ic] - pole[i-1]->freq[ip];
                                                  if(ftemp >= 0.0)
                                                  ftemp = ftemp/pole[i-1]->freq[ip];
                                                  else
                                                  ftemp = ftemp/pole[i]->freq[ic]; */
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
                    for(k=0, berr=0, ferr=0, fbias=0; k<nform; k++){
                        ic = fl[i]->cand[j][k];
                        if(ic >= 0){
                            if( !k ){		/* F1 candidate? */
                                ftemp = pole[i]->freq[ic];
                                merger = (domerge &&
                                        (ftemp == pole[i]->freq[fl[i]->cand[j][1]]))?
                                    merge_cost: 0.0;
                            }
                            berr += pole[i]->band[ic];
                            ferr += (fabs(pole[i]->freq[ic]-fnom[k])/fnom[k]);
                            fbias += pole[i]->freq[ic];
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
            /**************************************************************************/

            /* Pick the candidate in the final frame with the lowest cost. */
            /* Starting with that min.-cost cand., work back thru the lattice. */
            dmaxc = 0;
            dminc = 100;
            dcountc = dcountf = 0;
            for(mincan = -1, i=ps->length - 1; i>=0; i--){
                if(mincan < 0)		/* need to find best starting candidate? */
                    if(fl[i]->ncand){	/* have candidates at this frame? */
                        minerr = fl[i]->cumerr[0];
                        mincan = 0;
                        for(j=1; j<fl[i]->ncand; j++)
                            if( fl[i]->cumerr[j] < minerr ){
                                minerr = fl[i]->cumerr[j];
                                mincan = j;
                            }
                    }
                if(mincan >= 0){	/* if there is a "best" candidate at this frame */
                    if((j = fl[i]->ncand) > dmaxc) dmaxc = j;
                    else
                        if( j < dminc) dminc = j;
                    dcountc += j;
                    dcountf++;
                    for(j=0; j<nform; j++){
                        k = fl[i]->cand[mincan][j];
                        if(k >= 0){
                            fr[j][i] = pole[i]->freq[k];
                            ba[j][i] = pole[i]->band[k];
                        } else {		/* IF FORMANT IS MISSING... */
                            if(i < ps->length - 1){
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
                    for(j=0; j < nform; j++){
                        fr[j][i] = fnom[j];
                        ba[j][i] = NOBAND;
                    }
                }			/* note that mincan will remain =-1 if no candidates */
            }				/* end unpacking formant tracks from the dp lattice */
            /* Deallocate all the DP lattice work space. */
            for(i=ps->length - 1; i>=0; i--){
                if(fl[i]->ncand){
                    if(fl[i]->cand) {
                        for(j=0; j<fl[i]->ncand; j++) free(fl[i]->cand[j]);
                        free(fl[i]->cand);
                        free(fl[i]->cumerr);
                        free(fl[i]->prept);
                    }
                }
            }
            for(i=0; i<ps->length; i++)	free(fl[i]);
            free(fl);
            fl = 0;

            for(i=0; i<ps->length; i++) {
                free(pole[i]->freq);
                free(pole[i]->band);
                free(pole[i]);
            }
            free(pole);

            /* Deallocate space for the raw candidate aray. */
            for(i=0;i<MAXCAN;i++) free(pcan[i]);
            free(pcan);

            fbs = Snack_NewSound(ps->samprate, nform * 2);
            Snack_ResizeSoundStorage(fbs, ps->length);
            for (i = 0; i < ps->length; i++) {
                for (j = 0; j < nform * 2; j++) {
                    Snack_SetSample(fbs, j, i, fr[j][i]);
                }
            }
            fbs->length = ps->length;

            for(i = 0; i < nform*2; i++) free(fr[i]);
            free(fr);

            return(fbs);
        } else
            printf("Can't create a new Signal in dpform()\n");
    } else
        printf("Bad data pointers passed into dpform()\n");
    return(NULL);
    }

    /* lpc_poles.c */

    /* computation and I/O routines for dealing with LPC poles */

#define MAXORDER 30

/*************************************************************************/
static double integerize(double time, double freq) {
    int i;

    i = (int) (.5 + (freq * time));
    return(((double)i)/freq);
}

/**********************************************************************/
static double frand() {
    return (((double)rand())/(double)RAND_MAX);
}

/**********************************************************************/
/* a quick and dirty interface to bsa's stabilized covariance LPC */
#define NPM	30	/* max lpc order		*/

static int lpcbsa(int np, int wind, short *data, double *lpc, double *energy,
                  double preemp)
{
    static int i, mm, owind=0, wind1;
    static double w[1000];
    double rc[NPM],phi[NPM*NPM],shi[NPM],sig[1000];
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
    if((mm=dlpcwtd(sig,&wind1,lpc,&np,rc,phi,shi,&xl,w))!=np) {
        printf("LPCWTD error mm<np %d %d\n",mm,np);
        return(false);
    }
    return(true);
}

/*************************************************************************/
static sound_t *lpc_poles(sound_t *sp, double wdur, double frame_int, int lpc_ord,
                        double preemp, int lpc_type, int w_type)
{
    int i, j, size, step, nform, init, nfrm;
    pole_t **pole;
    double lpc_stabl = 70.0, energy, lpca[MAXORDER], normerr,
           *bap=NULL, *frp=NULL, *rhp=NULL;
    short *datap, *dporg;
    sound_t *lp;

    if(lpc_type == 1) { /* force "standard" stabilized covariance (ala bsa) */
        wdur = 0.025;
        preemp = exp(-62.831853 * 90. / sp->samprate); /* exp(-1800*pi*T) */
    }
    if((lpc_ord > MAXORDER) || (lpc_ord < 2)/* || (! ((short**)sp->data)[0])*/)
        return(NULL);
    /*  np = (char*)new_ext(sp->name,"pole");*/
    wdur = integerize(wdur,(double)sp->samprate);
    frame_int = integerize(frame_int,(double)sp->samprate);
    nfrm= 1 + (int) (((((double)sp->length)/sp->samprate) - wdur)/(frame_int));
    if(nfrm >= 1/*lp->buff_size >= 1*/) {
        size = (int) (.5 + (wdur * sp->samprate));
        step = (int) (.5 + (frame_int * sp->samprate));
        pole = malloc(nfrm/*lp->buff_size*/ * sizeof(pole_t*));
        datap = dporg = malloc(sizeof(short) * sp->length);
        for (i = 0; i < sp->length; i++) {
            datap[i] = (short) Snack_GetSample(sp, 0, i);
        }
        for(j=0, init=true/*, datap=((short**)sp->data)[0]*/; j < nfrm/*lp->buff_size*/;j++, datap += step){
            pole[j] = malloc(sizeof(pole_t));
            pole[j]->freq = frp = malloc(sizeof(double)*lpc_ord);
            pole[j]->band = bap = malloc(sizeof(double)*lpc_ord);

            switch(lpc_type) {
                case 0:
                    if(! lpc(lpc_ord,lpc_stabl,size,datap,lpca,rhp,NULL,&normerr,
                                &energy, preemp, w_type)){
                        printf("Problems with lpc in lpc_poles()");
                        break;
                    }
                    break;
                case 1:
                    if(! lpcbsa(lpc_ord,size,datap,lpca, &energy, preemp)){
                        printf("Problems with lpc in lpc_poles()");
                        break;
                    }
                    break;
                case 2:
                    {
                        int Ord = lpc_ord;
                        double alpha, r0;

                        w_covar(datap, &Ord, size, 0, lpca, &alpha, &r0, preemp, 0);
                        if((Ord != lpc_ord) || (alpha <= 0.0))
                            printf("Problems with covar(); alpha:%f  Ord:%d\n",alpha,Ord);
                        energy = sqrt(r0/(size-Ord));
                    }
                    break;
            }
            pole[j]->change = 0.0;
            /* don't waste time on low energy frames */
            if((pole[j]->rms = energy) > 1.0){
                formant(lpc_ord,(double)sp->samprate, lpca, &nform, frp, bap, init);
                pole[j]->npoles = nform;
                init=false;		/* use old poles to start next search */
            } else {			/* write out no pole frequencies */
                pole[j]->npoles = 0;
                init = true;		/* restart root search in a neutral zone */
            }
        } /* end LPC pole computation for all lp->buff_size frames */
        /*     lp->data = (caddr_t)pole;*/
        free(dporg);
        lp = Snack_NewSound((int)(1.0/frame_int), lpc_ord);
        Snack_ResizeSoundStorage(lp, nfrm);
        for (i = 0; i < nfrm; i++) {
            for (j = 0; j < lpc_ord; j++) {
                Snack_SetSample(lp, j, i, pole[i]->freq[j]);
            }
        }
        lp->length = nfrm;
        lp->pole = pole;
        return(lp);
    } else {
        printf("Bad buffer size in lpc_poles()\n");
    }
    return(NULL);
}

/*	Copyright (c) 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/* downsample.c */
/* a quick and dirty downsampler */

#define PI 3.1415927

/*      ----------------------------------------------------------      */
/* create the coefficients for a symmetric FIR lowpass filter using the
   window technique with a Hanning window. */
static int lc_lin_fir(double fc, int *nf, double *coef) {
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

    return(true);
}

/*      ----------------------------------------------------------      */

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

    for(i=ncoef-1, bufp=ic+ncoef-1, bufp2=co, buft = co+((ncoef-1)*2),
            integral = 0; i-- > 0; )
        if(!invert) *buft-- = *bufp2++ = *bufp--;
        else {
            integral += (stem = *bufp--);
            *buft-- = *bufp2++ = -stem;
        }
    if(!invert)  *buft-- = *bufp2++ = *bufp--; /* point of symmetry */
    else {
        integral *= 2;
        integral += *bufp;
        *buft-- = integral - *bufp;
    }
    /*         for(i=(ncoef*2)-2; i >= 0; i--) printf("\n%4d%7d",i,co[i]);  */
    for(i=ncoef-1, buft=mem; i-- > 0; ) *buft++ = 0;
    for(i=ncoef; i-- > 0; ) *buft++ = *buf++;
    l = 16384;
    m = 15;
    k = (ncoef << 1) -1;
    for(i=in_samps-ncoef; i-- > 0; ) {
        for(j=k, buft=mem, bufp=co, bufp2=mem+1, sum = 0; j-- > 0;
                *buft++ = *bufp2++ )
            sum += (((*bufp++ * *buft) + l) >> m);

        *--buft = *buf++;		/* new data to memory */
        *bufo++ = sum;
    }
    for(i=ncoef; i-- > 0; ) {	/* pad data end with zeros */
        for(j=k, buft=mem, bufp=co, bufp2=mem+1, sum = 0; j-- > 0;
                *buft++ = *bufp2++ )
            sum += (((*bufp++ * *buft) + l) >> m);
        *--buft = 0;
        *bufo++ = sum;
    }
}

/* ******************************************************************** */

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

/* ******************************************************************** */

static int dwnsamp(short *buf, int in_samps, short **buf2, int *out_samps,
                   int insert, int decimate, int ncoef, short *ic, int *smin,
                   int *smax)
{
    short  *bufp, *bufp2;
    short	*buft;
    int i, j, k, l, m;
    int imax, imin;

    if(!(*buf2 = buft = malloc(sizeof(short)*insert*in_samps))) {
        perror("ckalloc() in dwnsamp()");
        return(false);
    }

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
    return(true);
}

/*      ----------------------------------------------------------      */

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

/* ----------------------------------------------------------------------- */

static sound_t *Fdownsample(sound_t *s, double freq2, int start, int end) {
    short	*bufin, **bufout;
    static double	beta = 0.0, b[256];
    double	ratio_t, maxi, ratio, beta_new, freq1;
    static int	ncoeff = 127, ncoefft = 0, nbits = 15;
    static short	ic[256];
    int	insert, decimate, out_samps, smin, smax;
    sound_t *so;

    int i, j;

    freq1 = s->samprate;

    if((bufout = malloc(sizeof(short*)))) {
        bufin = malloc(sizeof(short) * (end - start + 1));
        for (i = start; i <= end; i++) {
            bufin[i-start] = (short) Snack_GetSample(s, 0, i);
        }

        ratio = freq2/freq1;
        ratprx(ratio,&insert,&decimate,10);
        ratio_t = ((double)insert)/((double)decimate);

        if(ratio_t > .99) return(s);

        freq2 = ratio_t * freq1;
        beta_new = (.5 * freq2)/(insert * freq1);

        if(beta != beta_new){
            beta = beta_new;
            if( !lc_lin_fir(beta,&ncoeff,b)) {
                printf("\nProblems computing interpolation filter\n");
                return(false);
            }
            maxi = (1 << nbits) - 1;
            j = (ncoeff/2) + 1;
            for(ncoefft = 0, i=0; i < j; i++){
                ic[i] = (int) (0.5 + (maxi * b[i]));
                if(ic[i]) ncoefft = i+1;
            }
        }				/*  endif new coefficients need to be computed */

        if(dwnsamp(bufin,end-start+1,bufout,&out_samps,insert,decimate,ncoefft,ic,
                    &smin,&smax)){
            /*      so->buff_size = so->file_size = out_samps;*/
            so = Snack_NewSound(0, s->nchannels);
            Snack_ResizeSoundStorage(so, out_samps);
            for (i = 0; i < out_samps; i++) {
                Snack_SetSample(so, 0, i, (*bufout)[i]);
            }
            so->length = out_samps;
            so->samprate = (int)freq2;
            free(*bufout);
            free(bufout);
            free(bufin);
            return(so);
        } else
            printf("Problems in dwnsamp() in downsample()\n");
    } else
        printf("Can't create a new Signal in downsample()\n");

    return(NULL);
}

/*      ----------------------------------------------------------      */

static sound_t *highpass(sound_t *s) {
    short *datain, *dataout;
    static short *lcf;
    static int len = 0;
    double scale, fn;
    int i;
    sound_t *so;

    /*  Header *h, *dup_header();*/

#define LCSIZ 101
    /* This assumes the sampling frequency is 10kHz and that the FIR
       is a Hanning function of (LCSIZ/10)ms duration. */

    datain = malloc(sizeof(short) * s->length);
    dataout = malloc(sizeof(short) * s->length);
    for (i = 0; i < s->length; i++) {
        datain[i] = (short) Snack_GetSample(s, 0, i);
    }

    if(!len) {		/* need to create a Hanning FIR? */
        lcf = malloc(sizeof(short) * LCSIZ);
        len = 1 + (LCSIZ/2);
        fn = PI * 2.0 / (LCSIZ - 1);
        scale = 32767.0/(.5 * LCSIZ);
        for(i=0; i < len; i++)
            lcf[i] = (short) (scale * (.5 + (.4 * cos(fn * ((double)i)))));
    }
    do_fir(datain,s->length,dataout,len,lcf,1); /* in downsample.c */
    so = Snack_NewSound(s->samprate, s->nchannels);
    if (so == NULL) return(NULL);
    Snack_ResizeSoundStorage(so, s->length);
    for (i = 0; i < s->length; i++) {
        Snack_SetSample(so, 0, i, dataout[i]);
    }
    so->length = s->length;
    free(dataout);
    free(datain);
    return(so);
}

void formantCmd(sound_t *s) {
    int nform, i,j, lpc_ord, lpc_type, w_type;
    double frame_int, wdur,
           ds_freq, nom_f1 = -10.0, preemp;
    sound_t *dssnd = NULL, *hpsnd = NULL, *polesnd = NULL;
    sound_t *formantsnd = NULL, *hpsrcsnd, *polesrcsnd;
    int startpos = 0, endpos = -1;

    lpc_ord = 12;
    lpc_type = 0;			/* use bsa's stabilized covariance if != 0 */
    w_type = 2;			/* window type: 0=rectangular; 1=Hamming; 2=cos**4 */
    ds_freq = 10000.0;
    wdur = .049;			/* for LPC analysis */
    frame_int = .01;
    preemp = .7;
    nform = 4;

    if (startpos < 0) startpos = 0;
    if (endpos >= (s->length - 1) || endpos == -1)
        endpos = s->length - 1;

    if (startpos > endpos)
        return;

    assert(!(nform > (lpc_ord-4)/2));
    assert(!(nform > MAXFORMANTS));

    w_type = 0;

    if(ds_freq < s->samprate) {
        dssnd = Fdownsample(s,ds_freq, startpos, endpos);
    }

    hpsrcsnd = (dssnd ? dssnd : s);

    if (preemp < 1.0) { /* be sure DC and rumble are gone! */
        hpsnd = highpass(hpsrcsnd);
    }

    polesrcsnd = (hpsnd ? hpsnd : s);

    polesnd = lpc_poles(polesrcsnd, wdur, frame_int, lpc_ord,
            preemp, lpc_type, w_type);

    formantsnd = dpform(polesnd, nform, nom_f1);

    if (dssnd) Snack_DeleteSound(dssnd);
    if (hpsnd) Snack_DeleteSound(hpsnd);
    Snack_DeleteSound(polesnd);

    for (i = 0; i < formantsnd->length; i += 1) {
        for (j = 0; j < nform * 2; j += 1)
            printf("%f\n", (double)(Snack_GetSample(formantsnd, j, i)));

        putchar('\n');
    }

    Snack_DeleteSound(formantsnd);
}
