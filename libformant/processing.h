// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014 Formant Industries, Inc.
//     Copyright (c) 1995 Entropic Research Laboratory, Inc.
//     Copyright (c) 1987 AT&T All Rights Reserved

#ifndef PROCESSING_H
#define PROCESSING_H

#include <assert.h>
#include <stddef.h>

#include "formant.h"

#define FORMANT_SAMPLE_RATE 10000

#define FORMANT_COUNT 4
#define MAX_FORMANTS 7

#define LPC_STABLE 70
#define LPC_ORDER 12
#define LPC_ORDER_MIN 2
#define LPC_ORDER_MAX 30
#define LPC_COEF (LPC_ORDER + 1)

// Nominal F1 frequency.
#define NOM_FREQ 0

static_assert(LPC_ORDER <= LPC_ORDER_MAX && LPC_ORDER >= LPC_ORDER_MIN,
    "LPC_ORDER out of bounds");

static_assert(FORMANT_COUNT <= MAX_FORMANTS,
    "FORMANT_COUNT out of bounds");

static_assert(FORMANT_COUNT <= (LPC_ORDER - 4) / 2,
    "FORMANT_COUNT too large");

int formant(double s_freq, double *lpca, size_t *n_form, double *freq,
            double *band, double *rr, double *ri);

void lpc(size_t wsize, const formant_sample_t *data, double *lpca, double *rms);

#endif
