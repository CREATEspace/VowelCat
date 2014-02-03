// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014 Formant Industries, Inc.
//     Copyright (c) 1995 Entropic Research Laboratory, Inc.
//     Copyright (c) 1987 AT&T All Rights Reserved

#ifndef PROCESSING_H
#define PROCESSING_H

int formant(int lpc_order, double s_freq, double *lpca, int *n_form,
            double *freq, double *band, double *rr, double *ri);

int w_covar(short *xx, int *m, int n, int istrt, double *y, double *alpha,
            double *r0, double preemp, int w_type);

int lpc(int lpc_ord, double lpc_stabl, int wsize, short *data, double *lpca,
        double *ar, double *lpck, double *normerr, double *rms, double preemp,
        int type);

int dlpcwtd(double *s, int *ls, double *p, int *np, double *c, double *phi,
            double *shi, double *xl, double *w);

#endif
