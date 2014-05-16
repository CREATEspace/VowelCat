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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define __USE_XOPEN
#include <math.h>

#include "processing.h"

/*
 * Compute the pp+1 autocorrelation lags of the windowsize samples in s.
 * Return the normalized autocorrelation coefficients in coef.
 * The rms is returned in rms.
 */
static void autoc(size_t windowsize, const formant_sample_t *s, double *coef,
                  size_t ncoef, double *rms)
{
    double sum, sum0;

    sum0 = 0;

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
        sum = 0;

        for (size_t j = 0; j < windowsize - i; j += 1)
            sum += s[j] * s[i + j];

        coef[i] = sum / sum0;
    }

    *rms = sqrt(sum0 / windowsize);
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

void lpc(size_t wsize, const formant_sample_t *data, double *lpca, double *rms) {
    double rho[LPC_COEF];

    autoc(wsize, data, rho, LPC_COEF, rms);

    /* add a little to the diagonal for stability */
    if (LPC_STABLE > 1.0) {
        double ffact = 1.0 / (1.0 + exp(-LPC_STABLE / 20.0 * log(10.0)));

        for (size_t i = 1; i < LPC_COEF; i++)
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
    int	    ord, ordm1, ordm2, itcnt, i, k, mmk, mmkp2, mmkp1, ntrys;
    double  err, p, q, delp, delq, den;
    double b[LPC_COEF];
    double c[LPC_COEF];
    double  lim0 = 0.5 * sqrt(DBL_MAX);

    for (ord = order; ord > 2; ord -= 2) {
        ordm1 = ord - 1;
        ordm2 = ord - 2;

        /* Here is a kluge to prevent UNDERFLOW! (Sometimes the near-zero
           roots left in rootr and/or rooti cause underflow here...	*/
        if (fabs(rootr[ordm1]) < 1.0e-10)
            rootr[ordm1] = 0.0;

        if (fabs(rooti[ordm1]) < 1.0e-10)
            rooti[ordm1] = 0.0;

        /* set initial guesses for quad factor */
        p = -2.0 * rootr[ordm1];
        q = rootr[ordm1]*rootr[ordm1] + rooti[ordm1]*rooti[ordm1];

        for (ntrys = 0; ntrys < MAX_TRYS; ntrys++) {
            int	found = false;

            for (itcnt = 0; itcnt < MAX_ITS; itcnt++) {
                double lim = lim0 / (1 + fabs(p) + fabs(q));

                b[ord] = a[ord];
                b[ordm1] = a[ordm1] - (p * b[ord]);

                c[ord] = b[ord];
                c[ordm1] = b[ordm1] - (p * c[ord]);

                for (k = 2; k <= ordm1; k++) {
                    mmk = ord - k;
                    mmkp2 = mmk+2;
                    mmkp1 = mmk+1;

                    b[mmk] = a[mmk] - (p* b[mmkp1]) - (q* b[mmkp2]);
                    c[mmk] = b[mmk] - (p* c[mmkp1]) - (q* c[mmkp2]);

                    if (b[mmk] > lim || c[mmk] > lim)
                        break;
                }

                /* normal exit from for (k ... */
                if (k > ordm1) {
                    b[0] = a[0] - p * b[1] - q * b[2];

                    if (b[0] <= lim)
                        k++;
                }

                /* Some coefficient exceeded lim; */
                /* potential overflow below. */
                if (k <= ord)
                    break;

                err = fabs(b[0]) + fabs(b[1]);

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
        for (i = 0; i <= ordm2; i++)
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
/* s_freq: the sampling frequency of the speech waveform data */
/* lpca: linear predictor coefficients */
/* freq: returned array of candidate formant frequencies */
/* band: returned array of candidate formant bandwidths */
int formant(double s_freq, double *lpca, size_t *n_form, double *freq,
            double *band, double *rr, double *ri)
{
    double  flo, pi2t, theta;
    size_t fc;

    /* was there a problem in the root finder? */
    if (!lbpoly(lpca, LPC_ORDER, rr, ri)) {
        *n_form = 0;
        return false;
    }

    pi2t = M_PI * 2.0 / s_freq;
    fc = 0;

    /* convert the z-plane locations to frequencies and bandwidths */
    for (size_t i = 0; i < LPC_ORDER; i++) {
        if (rr[i] == 0.0 && ri[i] == 0.0)
            continue;

        theta = atan2(ri[i], rr[i]);
        freq[fc] = fabs(theta / pi2t);
        band[fc] = 0.5 * s_freq * log(rr[i]*rr[i] + ri[i]*ri[i]) / M_PI;

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

    /* temporarily hold the folding frequency. */
    theta = s_freq / 2.0;

    /* order the poles by frequency (bubble) */
    for (size_t i = 0; i < fc - 1; i++) {
        for (size_t j = 0; j < fc - 1 - i; j++) {
            /* Force the real poles to the end of the list. */
            bool iscomp1 = freq[j] > 1.0 && freq[j] < theta;
            bool iscomp2 = freq[j + 1] > 1.0 && freq[j + 1] < theta;
            bool swit = freq[j] > freq[j + 1] && iscomp2;

            if (!(swit || (iscomp2 && !iscomp1)))
                continue;

            flo = band[j + 1];
            band[j + 1] = band[j];
            band[j] = flo;

            flo = freq[j + 1];
            freq[j + 1] = freq[j];
            freq[j] = flo;
        }
    }

    theta -= 1;
    size_t nform = 0;

    /* Now count the complex poles as formant candidates. */
    for (size_t i = 0; i < fc; i += 1)
        if (freq[i] > 1.0 && freq[i] < theta)
            nform += 1;

    *n_form = nform;

    return true;
}
