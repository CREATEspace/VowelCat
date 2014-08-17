// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014 Formant Industries, Inc.
//     Copyright (c) 1995 Entropic Research Laboratory, Inc.
//     Copyright (c) 1987 AT&T All Rights Reserved

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "processing.h"

/*	routine to solve ax=y with cholesky
    a - nxn matrix
    x,y -vectors
    */
static void dlwrtrn(double *a, int *n, double *x, double *y) {
    double *pxl,*pa,*py,*pyl,*pa1,*px;
    double sm;
    *x = *y / *a;
    pxl = x + 1;
    pyl = y + *n;
    pa = a + *n;
    for(py=y+1;py<pyl;py++,pxl++){
        sm = *py;
        pa1 = pa;
        for(px=x;px<pxl;px++)
            sm = sm - *(pa1++) * *px;
        pa += *n;
        *px = sm / *pa1;
    }
}

/*	convert ref to lpc
    c - ref
    a - polyn
    n - no of coef
    */
static void dreflpc(double *c, double *a, int *n) {
    double *pa1,*pa2,*pa3,*pa4,*pa5,*pc;
    double ta1;
    *a = 1.;
    *(a+1) = *c;
    pc = c; pa2 = a+ *n;
    for(pa1=a+2;pa1<=pa2;pa1++)
    {
        pc++;
        *pa1 = *pc;
        pa5 = a + (pa1-a)/2;
        pa4 = pa1 - 1;
        for(pa3=a+1;pa3<=pa5;pa3++,pa4--)
        {
            ta1 = *pa3 + *pc * *pa4;
            *pa4 = *pa4 + *pa3 * *pc;
            *pa3 = ta1;
        }
    }
}

/*	performs cholesky decomposition
    a - l * l(transpose)
    l - lower triangle
    det det(a)
    a - nxn matrix
    return - no of reduced elements
    results in lower half + diagonal
    upper half undisturbed.
    */
static int dchlsky(double *a, int *n, double *t, double *det) {
    double *pa_1,*pa_2,*pa_3,*pa_4,*pa_5,*pal,*pt;
    double sm;
    double sqrt();
    int m;
    *det = 1.;
    m = 0;
    pal = a + *n * *n;
    for(pa_1=a;pa_1<pal;pa_1+= *n){
        pa_3=pa_1;
        pt = t;
        for(pa_2=a;pa_2<=pa_1;pa_2+= *n){
            sm = *pa_3;	/*a(i,j)*/
            pa_5 = pa_2;
            for(pa_4=pa_1;pa_4<pa_3;pa_4++)
                sm =  sm - *pa_4 * *(pa_5++);
            if(pa_1==pa_2){
                if(sm<=0.)return(m);
                *pt = sqrt(sm);
                *det = *det * *pt;
                *(pa_3++) = *pt;
                m++;
                *pt = 1. / *pt;
                pt++;
            }
            else
                *(pa_3++) = sm * *(pt++);
        }
    }
    return(m);
}

/*	solve p*a=s using stabilized covariance method
    p - cov nxn matrix
    s - corrvec
    a lpc coef *a = 1.
    c - ref coefs
    */
static int dcovlpc(double *p, double *s, double *a, int *n, double *c) {
    double *pp,*ppl,*pa;
    double ee;
    double ps,thres,d;
    int m,n1;
    m = dchlsky(p,n,c,&d);
    dlwrtrn(p,n,c,s);
    thres = 1.0e-31;
    n1 = *n + 1;
    ps = *(a + *n);
    ppl = p + *n * m;
    m = 0;
    for(pp=p;pp<ppl;pp+=n1){
        if(*pp<thres)break;
        m++;
    }
    ee = ps;
    ppl = c + m; pa = a;
    for(pp=c;pp<ppl;pp++){
        ee = ee - *pp * *pp;
        if(ee<thres)break;
        *(pa++) = sqrt(ee);
    }
    m = pa - a;
    *c = - *c/sqrt(ps);
    ppl = c + m; pa = a;
    for(pp=c+1;pp<ppl;pp++)
        *pp = - *pp / *(pa++);
    dreflpc(c,a,&m);
    ppl = a + *n;
    for(pp=a+m+1;pp<=ppl;pp++)*pp=0.;
    return(m);
}

/* cov mat for wtd lpc	*/
static void dcwmtrx(double *s, int *ni, int *nl, int *np, double *phi,
                    double *shi, double *ps, double *w)
{
    double *pdl1,*pdl2,*pdl3,*pdl4,*pdl5,*pdl6,*pdll;
    double sm;
    int i,j;
    *ps = 0;
    for(pdl1=s+*ni,pdl2=w,pdll=s+*nl;pdl1<pdll;pdl1++,pdl2++)
        *ps += *pdl1 * *pdl1 * *pdl2;

    for(pdl3=shi,pdl4=shi+*np,pdl5=s+*ni;pdl3<pdl4;pdl3++,pdl5--){
        *pdl3 = 0.;
        for(pdl1=s+*ni,pdl2=w,pdll=s+*nl,pdl6=pdl5-1;
                pdl1<pdll;pdl1++,pdl2++,pdl6++)
            *pdl3 += *pdl1 * *pdl6 * *pdl2;

    }

    for(i=0;i<*np;i++)
        for(j=0;j<=i;j++){
            sm = 0.;
            for(pdl1=s+*ni-i-1,pdl2=s+*ni-j-1,pdl3=w,pdll=s+*nl-i-1;
                    pdl1<pdll;)
                sm += *pdl1++ * *pdl2++ * *pdl3++;

            *(phi + *np * i + j) = sm;
            *(phi + *np * j + i) = sm;
        }
}

/*	pred anal subroutine with ridge reg
    s - speech
    ls - length of s
    p - pred coefs
    np - polyn order
    c - ref coers
    phi - cov matrix
    shi - cov vect
    */
int dlpcwtd(double *s, int *ls, double *p, int *np, double *c, double *phi,
            double *shi, double *xl, double *w)
{
    double *pp2,*ppl2,*pc2,*pcl,*pph1,*pph2,*pph3,*pphl;
    int m,np1,mm;
    double d,pre,pre3,pre2,pre0,pss,thres;
    double ee;
    np1  =  *np  +  1;
    dcwmtrx(s,np,ls,np,phi,shi,&pss,w);
    if(*xl>=1.0e-4)
    {
        pph1 = phi; ppl2 = p + *np;
        for(pp2=p;pp2<ppl2;pp2++){
            *pp2 = *pph1;
            pph1 += np1;
        }
        *ppl2 = pss;
        mm = dchlsky(phi,np,c,&d);
        dlwrtrn(phi,np,c,shi);
        ee = pss;
        thres = 0.;
        pph1 = phi; pcl = c + mm;
        for(pc2=c;pc2<pcl;pc2++)
        {
            if(*pph1<thres)break;
            ee = ee - *pc2 * *pc2;
            if(ee<thres)break;
        }
        pre = ee * *xl;
        pphl = phi + *np * *np;
        for(pph1=phi+1;pph1<pphl;pph1+=np1)
        {
            pph2 = pph1;
            for(pph3=pph1+ *np-1;pph3<pphl;pph3+= *np)
            {
                *pph3 = *(pph2++);
            }
        }
        pp2 = p; pre3 = .375 * pre; pre2 = .25 * pre; pre0 = .0625 * pre;
        for(pph1=phi;pph1<pphl;pph1+=np1)
        {
            *pph1 = *(pp2++) + pre3;
            if((pph2=pph1- *np)>phi)
                *pph2 = *(pph1-1) = *pph2 - pre2;
            if((pph3=pph2- *np)>phi)
                *pph3 = *(pph1-2) = *pph3 + pre0;
        }
        *shi -= pre2;
        *(shi+1) += pre0;
        *(p+ *np) = pss + pre3;
    }
    m = dcovlpc(phi,shi,p,np,c);
    return(m);
}

/*
 *
 *	An implementation of the Le Roux - Gueguen PARCOR computation.
 *	See: IEEE/ASSP June, 1977 pp 257-259.
 *
 *	Author: David Talkin	Jan., 1984
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#ifndef DBL_MAX
#define DBL_MAX 1.7976931348623157E+308
#endif

#define MAXORDER	60	/* maximum permissible LPC order */

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

static void rwindow(short *din, double *dout, int n, double preemp) {
    short *p;

    /* If preemphasis is to be performed,  this assumes that there are n+1 valid
       samples in the input buffer (din). */
    if(preemp != 0.0) {
        for( p=din+1; n-- > 0; )
            *dout++ = (double)(*p++) - (preemp * *din++);
    } else {
        for( ; n-- > 0; )
            *dout++ =  *din++;
    }
}

static void cwindow(short *din, double *dout, int n, double preemp) {
    int i;
    short *p;
    int wsize = 0;
    double *wind=NULL;
    double *q, co;

    if(wsize != n) {		/* Need to create a new cos**4 window? */
        double arg, half=0.5;

        if(wind) wind = realloc((void *)wind,n*sizeof(double));
        else wind = malloc(n*sizeof(double));
        wsize = n;
        for(i=0, arg=3.1415927*2.0/(wsize), q=wind; i < n; ) {
            co = half*(1.0 - cos((half + (double)i++) * arg));
            *q++ = co * co * co * co;
        }
    }
    /* If preemphasis is to be performed,  this assumes that there are n+1 valid
       samples in the input buffer (din). */
    if(preemp != 0.0) {
        for(i=n, p=din+1, q=wind; i-- > 0; )
            *dout++ = *q++ * ((double)(*p++) - (preemp * *din++));
    } else {
        for(i=n, q=wind; i-- > 0; )
            *dout++ = *q++ * *din++;
    }
}

static void hwindow(short *din, double *dout, int n, double preemp) {
    int i;
    short *p;
    int wsize = 0;
    double *wind=NULL;
    double *q;

    if(wsize != n) {		/* Need to create a new Hamming window? */
        double arg, half=0.5;

        if(wind) wind = realloc((void *)wind,n*sizeof(double));
        else wind = malloc(n*sizeof(double));
        wsize = n;
        for(i=0, arg=3.1415927*2.0/(wsize), q=wind; i < n; )
            *q++ = (.54 - .46 * cos((half + (double)i++) * arg));
    }
    /* If preemphasis is to be performed,  this assumes that there are n+1 valid
       samples in the input buffer (din). */
    if(preemp != 0.0) {
        for(i=n, p=din+1, q=wind; i-- > 0; )
            *dout++ = *q++ * ((double)(*p++) - (preemp * *din++));
    } else {
        for(i=n, q=wind; i-- > 0; )
            *dout++ = *q++ * *din++;
    }
}

static void hnwindow(short *din, double *dout, int n, double preemp) {
    int i;
    short *p;
    int wsize = 0;
    double *wind=NULL;
    double *q;

    if(wsize != n) {		/* Need to create a new Hamming window? */
        double arg, half=0.5;

        if(wind) wind = realloc((void *)wind,n*sizeof(double));
        else wind = malloc(n*sizeof(double));
        wsize = n;
        for(i=0, arg=3.1415927*2.0/(wsize), q=wind; i < n; )
            *q++ = (half - half * cos((half + (double)i++) * arg));
    }
    /* If preemphasis is to be performed,  this assumes that there are n+1 valid
       samples in the input buffer (din). */
    if(preemp != 0.0) {
        for(i=n, p=din+1, q=wind; i-- > 0; )
            *dout++ = *q++ * ((double)(*p++) - (preemp * *din++));
    } else {
        for(i=n, q=wind; i-- > 0; )
            *dout++ = *q++ * *din++;
    }
}

static void w_window(short *din, double *dout, int n, double preemp,
                     window_type_t type)
{
    switch (type) {
    case WINDOW_TYPE_RECTANGULAR:
        rwindow(din, dout, n, preemp);
    break;

    case WINDOW_TYPE_HAMMING:
        hwindow(din, dout, n, preemp);
    break;

    case WINDOW_TYPE_COS:
        cwindow(din, dout, n, preemp);
    break;

    case WINDOW_TYPE_HANNING:
        hnwindow(din, dout, n, preemp);
    break;

    case WINDOW_TYPE_INVALID:
    break;
    }
}

/*
 * Compute the pp+1 autocorrelation lags of the windowsize samples in s.
 * Return the normalized autocorrelation coefficients in r.
 * The rms is returned in e.
 */
static void autoc(size_t windowsize, double *s, size_t p, double *r, double *e) {
    size_t i, j;
    double *q, *t, sum, sum0;

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
        for( sum=0., j=0, q=s, t=s+i; j+i < windowsize; j++, q++, t++){
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
static void durbin (double *r, double *k, double *a, int p, double *ex) {
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

void lpc(size_t lpc_ord, double lpc_stabl, size_t wsize, short *data, double *lpca,
         double *ar, double *lpck, double *normerr, double *rms, double preemp,
         window_type_t type)
{
    double *dwind;
    double rho[MAXORDER+1], k[MAXORDER], a[MAXORDER+1],*r,*kp,*ap,en,er;
    double wfact = 1.0;

    dwind = malloc(wsize*sizeof(double));

    w_window(data, dwind, wsize, preemp, type);
    if(!(r = ar)) r = rho;
    if(!(kp = lpck)) kp = k;
    if(!(ap = lpca)) ap = a;
    autoc( wsize, dwind, lpc_ord, r, &en );
    if(lpc_stabl > 1.0) { /* add a little to the diagonal for stability */
        size_t i;
        double ffact;
        ffact =1.0/(1.0 + exp((-lpc_stabl/20.0) * log(10.0)));
        for(i=1; i <= lpc_ord; i++) rho[i] = ffact * r[i];
        *rho = *r;
        r = rho;
        if(ar)
            for(i=0;i<=lpc_ord; i++) ar[i] = r[i]; /* copy out for possible use later */
    }
    durbin ( r, kp, &ap[1], lpc_ord, &er);

    *ap = 1.0;
    if(rms) *rms = en/wfact;
    if(normerr) *normerr = er;

    free(dwind);
}

/* covariance LPC analysis; originally from Markel and Gray */
/* (a translation from the fortran) */
int w_covar(short *xx, int *m, int n, int istrt, double *y, double *alpha,
            double *r0, double preemp, window_type_t w_type)
{
    double *x=NULL;
    int nold = 0;
    int mold = 0;
    double *b = NULL, *beta = NULL, *grc = NULL, *cc = NULL, gam,s;
    int ibeg, ibeg1, ibeg2, ibegmp, np0, ibegm1, msq, np, np1, mf, jp, ip,
        mp, i, j, minc, n1, n2, n3, npb, msub, mm1, isub, m2;
    int mnew = 0;

    if((n+1) > nold) {
        if(x) free((void *)x);
        x = NULL;
        x = malloc((n+1)*sizeof(double));
        memset(x, 0, (n+1) * sizeof(double));
        nold = n+1;
    }

    if(*m > mold) {
        if(b) free((void *)b); if(beta) free((void *)beta); if (grc) free((void *)grc); if (cc) free((void *)cc);
        mnew = *m;

        b = malloc(sizeof(double)*((mnew+1)*(mnew+1)/2));
        beta = malloc(sizeof(double)*(mnew+3));
        grc = malloc(sizeof(double)*(mnew+3));
        cc = malloc(sizeof(double)*(mnew+3));

        mold = mnew;
    }

    w_window(xx, x, n, preemp, w_type);

    ibeg = istrt - 1;
    ibeg1 = ibeg + 1;
    mp = *m + 1;
    ibegm1 = ibeg - 1;
    ibeg2 = ibeg + 2;
    ibegmp = ibeg + mp;
    i = *m;
    msq = ( i + i*i)/2;
    for (i=1; i <= msq; i++) b[i] = 0.0;
    *alpha = 0.0;
    cc[1] = 0.0;
    cc[2] = 0.0;
    for(np=mp; np <= n; np++) {
        np1 = np + ibegm1;
        np0 = np + ibeg;
        *alpha += x[np0] * x[np0];
        cc[1] += x[np0] * x[np1];
        cc[2] += x[np1] * x[np1];
    }
    *r0 = *alpha;
    b[1] = 1.0;
    beta[1] = cc[2];
    grc[1] = -cc[1]/cc[2];
    y[0] = 1.0;
    y[1] = grc[1];
    *alpha += grc[1]*cc[1];
    if( *m <= 1) return(false);		/* need to correct indices?? */
    mf = *m;
    for( minc = 2; minc <= mf; minc++) {
        for(j=1; j <= minc; j++) {
            jp = minc + 2 - j;
            n1 = ibeg1 + mp - jp;
            n2 = ibeg1 + n - minc;
            n3 = ibeg2 + n - jp;
            cc[jp] = cc[jp - 1] + x[ibegmp-minc]*x[n1] - x[n2]*x[n3];
        }
        cc[1] = 0.0;
        for(np = mp; np <= n; np++) {
            npb = np + ibeg;
            cc[1] += x[npb-minc]*x[npb];
        }
        msub = (minc*minc - minc)/2;
        mm1 = minc - 1;
        b[msub+minc] = 1.0;
        for(ip=1; ip <= mm1; ip++) {
            isub = (ip*ip - ip)/2;
            if(beta[ip] <= 0.0) {
                *m = minc-1;
                return(true);
            }
            gam = 0.0;
            for(j=1; j <= ip; j++)
                gam += cc[j+1]*b[isub+j];
            gam /= beta[ip];
            for(jp=1; jp <= ip; jp++)
                b[msub+jp] -= gam*b[isub+jp];
        }
        beta[minc] = 0.0;
        for(j=1; j <= minc; j++)
            beta[minc] += cc[j+1]*b[msub+j];
        if(beta[minc] <= 0.0) {
            *m = minc-1;
            return(true);
        }
        s = 0.0;
        for(ip=1; ip <= minc; ip++)
            s += cc[ip]*y[ip-1];
        grc[minc] = -s/beta[minc];
        for(ip=1; ip < minc; ip++) {
            m2 = msub+ip;
            y[ip] += grc[minc]*b[m2];
        }
        y[minc] = grc[minc];
        s = grc[minc]*grc[minc]*beta[minc];
        *alpha -= s;
        if(*alpha <= 0.0) {
            if(minc < *m) *m = minc;
            return(true);
        }
    }
    return(true);
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
