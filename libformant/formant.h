/*
 * This software has been licensed to the Centre of Speech Technology, KTH
 * by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
 * file BSD.txt, which is a BSD style license.
 *
 *    "Copyright (c) 1987-1990  AT&T, Inc.
 *    "Copyright (c) 1986-1990  Entropic Speech, Inc.
 *    "Copyright (c) 1990-1991  Entropic Research Laboratory, Inc.
 *                   All rights reserved"
 */

#ifndef FORMANT_H
#define FORMANT_H

typedef short sample_t;
typedef float storage_t;

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

typedef struct sound_t {
    size_t sample_rate;
    size_t n_channels;
    // The number of samples per channel in blocks.
    size_t n_samples;
    storage_t *blocks;

    pole_t **pole;
} sound_t;

void sound_init(sound_t *s, size_t sample_rate, size_t n_channels);
void sound_destroy(sound_t *s);
void sound_load_samples(sound_t *s, const short *samples, size_t n_samples);
void sound_calc_formants(sound_t *s);

static inline storage_t sound_get_sample(const sound_t *s, size_t chan, size_t i) {
    return s->blocks[i * s->n_channels + chan];
}

#endif
