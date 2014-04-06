// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014 Formant Industries, Inc.
//     Copyright (c) 1995 Entropic Research Laboratory, Inc.
//     Copyright (c) 1987 AT&T All Rights Reserved

/*
 *
 *	An implementation of the Le Roux - Gueguen PARCOR computation.
 *	See: IEEE/ASSP June, 1977 pp 257-259.
 *
 *	Author: David Talkin	Jan., 1984
 *
 */

#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "processing.h"

#ifndef DBL_MAX
#define DBL_MAX 1.7976931348623157E+308
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

#define MAXORDER	60	/* maximum permissible LPC order */

/*
 * Compute the pp+1 autocorrelation lags of the windowsize samples in s.
 * Return the normalized autocorrelation coefficients in r.
 * The rms is returned in e.
 */
static void autoc(size_t windowsize, const short *s, size_t p, double *r, double *e) {
    size_t i, j;
    double sum, sum0;
    const short *q, *t;

    for ( i=0, q=s, sum0=0.; i< windowsize; q++, i++){
        sum0 += (*q) * (*q);
    }
    *r = 1.;  /* r[0] will always =1. */
    if ( sum0 == 0.){   /* No energy: fake low-energy white noise. */
        *e = 1.;   /* Arbitrarily assign 1 to rms. */
        /* Now fake autocorrelation of white noise. */
        for ( i=1; i<=p; i++){
            r[i] = 0.;
        }
        return;
    }
    for( i=1; i <= p; i++){
        for( sum=0., j=0, q=s, t=s+i; j < (windowsize)-i; j++, q++, t++){
            sum += (*q) * (*t);
        }
        *(++r) = sum/sum0;
    }
    *e = sqrt(sum0/windowsize);
}

/*
 * Compute the AR and PARCOR coefficients using Durbin's recursion.
 * Note: Durbin returns the coefficients in normal sign format.
 *	(i.e. a[0] is assumed to be = +1.)
 */
static void durbin (const double *r, double *k, double *a, int p, double *ex) {
    double b[MAXORDER];
    int i, j;
    double e, s;

    e = *r;
    *k = -r[1]/e;
    *a = *k;
    e *= (1. - (*k) * (*k));
    for ( i=1; i < p; i++){
        s = 0;
        for ( j=0; j<i; j++){
            s -= a[j] * r[i-j];
        }
        k[i] = ( s - r[i+1] )/e;
        a[i] = k[i];
        for ( j=0; j<=i; j++){
            b[j] = a[j];
        }
        for ( j=0; j<i; j++){
            a[j] += k[i] * b[i-j-1];
        }
        e *= ( 1. - (k[i] * k[i]) );
    }
    *ex = e;
}

void lpc(size_t lpc_ord, double lpc_stabl, size_t wsize, const short *data,
         double *lpca, double *normerr, double *rms)
{
    double rho[MAXORDER+1], k[MAXORDER],en,er;

    autoc( wsize, data, lpc_ord, rho, &en );
    if(lpc_stabl > 1.0) { /* add a little to the diagonal for stability */
        size_t i;
        double ffact;
        ffact =1.0/(1.0 + exp((-lpc_stabl/20.0) * log(10.0)));
        for(i=1; i <= lpc_ord; i++) rho[i] = ffact * rho[i];
    }
    durbin ( rho, k, &lpca[1], lpc_ord, &er);

    *lpca = 1.0;
    *rms = en;
    *normerr = er;
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
    double  sqrt(), numi;
    double  den, y;

    if(a == 0.0){
        if(b == 0){
            return(false);
        }
        *r1r = -c/b;
        *r1i = *r2r = *r2i = 0;
        return(true);
    }
    numi = b*b - (4.0 * a * c);
    if(numi >= 0.0) {
        /*
         * Two forms of the quadratic formula:
         *  -b + sqrt(b^2 - 4ac)           2c
         *  ------------------- = --------------------
         *           2a           -b - sqrt(b^2 - 4ac)
         * The r.h.s. is numerically more accurate when
         * b and the square root have the same sign and
         * similar magnitudes.
         */
        *r1i = *r2i = 0.0;
        if(b < 0.0) {
            y = -b + sqrt(numi);
            *r1r = y / (2.0 * a);
            *r2r = (2.0 * c) / y;
        }
        else {
            y = -b - sqrt(numi);
            *r1r = (2.0 * c) / y;
            *r2r = y / (2.0 * a);
        }
        return(true);
    }
    else {
        den = 2.0 * a;
        *r1i = sqrt( -numi )/den;
        *r2i = -*r1i;
        *r2r = *r1r = -b/den;
        return(true);
    }
}

/* return false on error */
/* a: coeffs. of the polynomial (increasing order) */
/* order: the order of the polynomial */
/* rootr, rooti: the real and imag. roots of the polynomial */
/* Rootr and rooti are assumed to contain starting points for the root
   search on entry to lbpoly(). */
static int lbpoly(double *a, int order, double *rootr, double *rooti) {
    int	    ord, ordm1, ordm2, itcnt, i, k, mmk, mmkp2, mmkp1, ntrys;
    double  err, p, q, delp, delq, b[MAXORDER], c[MAXORDER], den;
    double  lim0 = 0.5*sqrt(DBL_MAX);

    for(ord = order; ord > 2; ord -= 2){
        ordm1 = ord-1;
        ordm2 = ord-2;
        /* Here is a kluge to prevent UNDERFLOW! (Sometimes the near-zero
           roots left in rootr and/or rooti cause underflow here...	*/
        if(fabs(rootr[ordm1]) < 1.0e-10) rootr[ordm1] = 0.0;
        if(fabs(rooti[ordm1]) < 1.0e-10) rooti[ordm1] = 0.0;
        p = -2.0 * rootr[ordm1]; /* set initial guesses for quad factor */
        q = (rootr[ordm1] * rootr[ordm1]) + (rooti[ordm1] * rooti[ordm1]);
        for(ntrys = 0; ntrys < MAX_TRYS; ntrys++)
        {
            int	found = false;

            for(itcnt = 0; itcnt < MAX_ITS; itcnt++)
            {
                double	lim = lim0 / (1 + fabs(p) + fabs(q));

                b[ord] = a[ord];
                b[ordm1] = a[ordm1] - (p * b[ord]);
                c[ord] = b[ord];
                c[ordm1] = b[ordm1] - (p * c[ord]);
                for(k = 2; k <= ordm1; k++){
                    mmk = ord - k;
                    mmkp2 = mmk+2;
                    mmkp1 = mmk+1;
                    b[mmk] = a[mmk] - (p* b[mmkp1]) - (q* b[mmkp2]);
                    c[mmk] = b[mmk] - (p* c[mmkp1]) - (q* c[mmkp2]);
                    if (b[mmk] > lim || c[mmk] > lim)
                        break;
                }
                if (k > ordm1) { /* normal exit from for(k ... */
                    /* ????	b[0] = a[0] - q * b[2];	*/
                    b[0] = a[0] - p * b[1] - q * b[2];
                    if (b[0] <= lim) k++;
                }
                if (k <= ord)	/* Some coefficient exceeded lim; */
                    break;	/* potential overflow below. */

                err = fabs(b[0]) + fabs(b[1]);

                if(err <= MAX_ERR) {
                    found = true;
                    break;
                }

                den = (c[2] * c[2]) - (c[3] * (c[1] - b[1]));
                if(den == 0.0)
                    break;

                delp = ((c[2] * b[1]) - (c[3] * b[0]))/den;
                delq = ((c[2] * b[0]) - (b[1] * (c[1] - b[1])))/den;

                p += delp;
                q += delq;

            } /* for(itcnt... */

            if (found)		/* we finally found the root! */
                break;
            else { /* try some new starting values */
                p = ((double)rand() - 0.5*RAND_MAX)/(double)RAND_MAX;
                q = ((double)rand() - 0.5*RAND_MAX)/(double)RAND_MAX;
            }

        } /* for(ntrys... */
        if((itcnt >= MAX_ITS) && (ntrys >= MAX_TRYS)){
            return(false);
        }

        if(!qquad(1.0, p, q,
                    &rootr[ordm1], &rooti[ordm1], &rootr[ordm2], &rooti[ordm2]))
            return(false);

        /* Update the coefficient array with the coeffs. of the
           reduced polynomial. */
        for( i = 0; i <= ordm2; i++) a[i] = b[i+2];
    }

    if(ord == 2){		/* Is the last factor a quadratic? */
        if(!qquad(a[2], a[1], a[0],
                    &rootr[1], &rooti[1], &rootr[0], &rooti[0]))
            return(false);
        return(true);
    }
    if(ord < 1) {
        return(false);
    }

    if( a[1] != 0.0) rootr[0] = -a[0]/a[1];
    else {
        rootr[0] = 100.0;	/* arbitrary recovery value */
    }
    rooti[0] = 0.0;

    return(true);
}

/* Find the roots of the LPC denominator polynomial and convert the z-plane
   zeros to equivalent resonant frequencies and bandwidths.	*/
/* The complex poles are then ordered by frequency.  */
/* lpc_ord: order of the LP model */
/* n_form: number of COMPLEX roots of the LPC polynomial */
/* init: preset to true if no root candidates are available */
/* s_freq: the sampling frequency of the speech waveform data */
/* lpca: linear predictor coefficients */
/* freq: returned array of candidate formant frequencies */
/* band: returned array of candidate formant bandwidths */
int formant(int lpc_order, double s_freq, double *lpca, int *n_form,
            double *freq, double *band, double *rr, double *ri)
{
    double  flo, pi2t, theta;
    int	i,ii,iscomp1,iscomp2,fc,swit;

    if(! lbpoly(lpca,lpc_order,rr,ri)){ /* find the roots of the LPC polynomial */
        *n_form = 0;		/* was there a problem in the root finder? */
        return(false);
    }

    pi2t = M_PI * 2.0 /s_freq;

    /* convert the z-plane locations to frequencies and bandwidths */
    for(fc=0, ii=0; ii < lpc_order; ii++){
        if((rr[ii] != 0.0)||(ri[ii] != 0.0)){
            theta = atan2(ri[ii],rr[ii]);
            freq[fc] = fabs(theta / pi2t);
            if((band[fc] = 0.5 * s_freq *
                        log(((rr[ii] * rr[ii]) + (ri[ii] * ri[ii])))/M_PI) < 0.0)
                band[fc] = -band[fc];
            fc++;			/* Count the number of real and complex poles. */

            if((rr[ii] == rr[ii+1])&&(ri[ii] == -ri[ii+1]) /* complex pole? */
                    && (ri[ii] != 0.0)) ii++; /* if so, don't duplicate */
        }
    }

    /* Now order the complex poles by frequency.  Always place the (uninteresting)
       real poles at the end of the arrays. 	*/
    theta = s_freq/2.0;		/* temporarily hold the folding frequency. */
    for(i=0; i < fc -1; i++){	/* order the poles by frequency (bubble) */
        for(ii=0; ii < fc -1 -i; ii++){
            /* Force the real poles to the end of the list. */
            iscomp1 = (freq[ii] > 1.0) && (freq[ii] < theta);
            iscomp2 = (freq[ii+1] > 1.0) && (freq[ii+1] < theta);
            swit = (freq[ii] > freq[ii+1]) && iscomp2 ;
            if(swit || (iscomp2 && ! iscomp1)){
                flo = band[ii+1];
                band[ii+1] = band[ii];
                band[ii] = flo;
                flo = freq[ii+1];
                freq[ii+1] = freq[ii];
                freq[ii] = flo;
            }
        }
    }
    /* Now count the complex poles as formant candidates. */
    for(i=0, theta = theta - 1.0, ii=0 ; i < fc; i++)
        if( (freq[i] > 1.0) && (freq[i] < theta) ) ii++;
    *n_form = ii;

    return(true);
}
