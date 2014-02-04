// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014 Formant Industries, Inc.
//     Copyright (c) 1995 Entropic Research Laboratory, Inc.
//     Copyright (c) 1987 AT&T All Rights Reserved

#ifndef PROCESSING_H
#define PROCESSING_H

#include <stddef.h>

typedef enum {
    WINDOW_TYPE_RECTANGULAR,
    WINDOW_TYPE_HAMMING,
    WINDOW_TYPE_COS,
    WINDOW_TYPE_HANNING,

    WINDOW_TYPE_INVALID,
} window_type_t;

int formant(int lpc_order, double s_freq, double *lpca, int *n_form,
            double *freq, double *band, double *rr, double *ri);

int w_covar(short *xx, int *m, int n, int istrt, double *y, double *alpha,
            double *r0, double preemp, window_type_t w_type);

void lpc(size_t lpc_ord, double lpc_stabl, size_t wsize, short *data, double *lpca,
         double *ar, double *lpck, double *normerr, double *rms, double preemp,
         window_type_t type);

int dlpcwtd(double *s, int *ls, double *p, int *np, double *c, double *phi,
            double *shi, double *xl, double *w);

#endif
