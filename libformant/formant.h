// This software has been licensed to the Centre of Speech Technology, KTH
// by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
// file BSD.txt, which is a BSD style license.
//
//     Copyright (c) 2014  Formant Industries, Inc.
//     Copyright (c) 1990-1991  Entropic Research Laboratory, Inc.
//         All rights reserved
//     Copyright (c) 1987-1990  AT&T, Inc.
//     Copyright (c) 1986-1990  Entropic Speech, Inc.

#ifndef FORMANT_H
#define FORMANT_H

#include <stdbool.h>
#include <stddef.h>

#include "processing.h"

// How input samples are represented.
typedef short formant_sample_t;

// Parameters for calculating formants.
typedef struct {
    // Sample rate to downsample to.
    double downsample_rate;
} formant_opts_t;

// Initialize the given options to (wavesurfer) defaults.
void formant_opts_init(formant_opts_t *opts);

typedef struct {   /* structure to hold raw LPC analysis data */
    double rms;    /* rms for current LPC analysis frame */
    double rms2;    /* rms for current F0 analysis frame */
    double f0;     /* fundamental frequency estimate for this frame */
    double pv;		/* probability that frame is voiced */
    size_t npoles; /* # of complex poles from roots of LPC polynomial */
    double *freq;  /* array of complex pole frequencies (Hz) */
    double *band;  /* array of complex pole bandwidths (Hz) */
} pole_t;

// A raw audio segment.
typedef struct sound_t {
    // Sample rate of the audio data in Hz.
    size_t sample_rate;
    // Number of channels (1 for mono, 2 for stereo, and so on).
    size_t channel_count;
    // The number of samples per channel in the audio data.
    size_t sample_count;
    // The audio data itself.
    formant_sample_t *samples;
} sound_t;

// Initialize the given sound to a default state.
void sound_init(sound_t *s);

// Reset the given sound to have the given sample rate and channel setup.
void sound_reset(sound_t *s, size_t sample_rate, size_t channel_count);

// Release the memory held by the given sound.
void sound_destroy(sound_t *s);

// Resize the given sound so it can hold the given number of samples.
void sound_resize(sound_t *s, size_t sample_count);

// Load a buffer of samples into the given sound. Note that sample_count is the
// total number of samples in the buffer, not per channel.
void sound_load_samples(sound_t *s, const formant_sample_t *samples, size_t sample_count);

// Downsample the given sound's sample rate to freq2.
void sound_downsample(sound_t *s, size_t freq2);

// Calculate the formants for the samples in the given sound. Return true if the
// formants were calculated successfully and false otherwise.
//
// The sound is modified in place such that
//
//  - sample_count is set to the number of formants calculated
//  - channel_count is set to 2 * sample_count
//
void sound_calc_formants(sound_t *s, const formant_opts_t *opts);

// Get the i'th F1 formant.
static inline formant_sample_t sound_f1(const sound_t *s) {
    return s->samples[0];
}

// Get the i'th F2 formant.
static inline formant_sample_t sound_f2(const sound_t *s) {
    return s->samples[1];
}

#endif
