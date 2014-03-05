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
    // Number of formants to calculate.
    size_t n_formants;

    // Sample rate to downsample to.
    double downsample_rate;
    // Preemphasis to perform on sound.
    double pre_emph_factor;

    // The type of window function to use.
    window_type_t window_type;

    // Duration of the window function in seconds.
    double window_dur;
    // Duration of the window frame in seconds.
    double frame_dur;

    // The type of LPC function to use.
    enum {
        LPC_TYPE_NORMAL,
        LPC_TYPE_BSA,
        LPC_TYPE_COVAR,

        LPC_TYPE_INVALID,
    } lpc_type;

    // XXX: not sure what these do.
    size_t lpc_order;
    double nom_freq;
} formant_opts_t;

// Initialize the given options to (wavesurfer) defaults.
void formant_opts_init(formant_opts_t *opts);

// Process and validate the given options. The options must be ran through this
// function before being passed into sound_calc_formants.
bool formant_opts_process(formant_opts_t *opts);

// A raw audio segment.
typedef struct sound_t {
    // Sample rate of the audio data in Hz.
    size_t sample_rate;
    // Number of channels (1 for mono, 2 for stereo, and so on).
    size_t n_channels;
    // The number of samples per channel in the audio data.
    size_t n_samples;
    // The audio data itself.
    formant_sample_t *samples;
} sound_t;

// Initialize the given sound to a default state.
void sound_init(sound_t *s);

// Reset the given sound to have the given sample rate and channel setup.
void sound_reset(sound_t *s, size_t sample_rate, size_t n_channels);

// Release the memory held by the given sound.
void sound_destroy(sound_t *s);

// Resize the given sound so it can hold the given number of samples.
void sound_resize(sound_t *s, size_t n_samples);

// Load a buffer of samples into the given sound. Note that n_samples is the
// total number of samples in the buffer, not per channel.
void sound_load_samples(sound_t *s, const formant_sample_t *samples, size_t n_samples);

// Calculate the formants for the samples in the given sound. Return true if the
// formants were calculated successfully and false otherwise.
//
// The sound is modified in place such that
//
//  - n_samples is set to the number of formants calculated
//  - n_channels is set to 2 * n_samples
//
bool sound_calc_formants(sound_t *s, const formant_opts_t *opts);

// Get the i'th sample in the given channel.
static inline formant_sample_t sound_get_sample(const sound_t *s, size_t chan, size_t i) {
    return s->samples[i * s->n_channels + chan];
}

// Get the i'th F1 formant.
static inline formant_sample_t sound_get_f1(const sound_t *s, size_t i) {
    return sound_get_sample(s, 0, i);
}

// Get the i'th F2 formant.
static inline formant_sample_t sound_get_f2(const sound_t *s, size_t i) {
    return sound_get_sample(s, 1, i);
}

#endif
