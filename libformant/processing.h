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

#include "formant.h"

int formant(double *lpca, size_t *n_form, double *freq, double *band, double *rr,
            double *ri);

void lpc(size_t wsize, const formant_sample_t *data, double *lpca, double *rms);

#endif
