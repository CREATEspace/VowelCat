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
typedef short sample_t;
// How samples are stored in the sound structure.
typedef float storage_t;

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
void formant_opts_process(formant_opts_t *opts);

// A raw audio segment.
typedef struct sound_t {
    // Sample rate of the audio data in Hz.
    size_t sample_rate;
    // Number of channels (1 for mono, 2 for stereo, and so on).
    size_t n_channels;
    // The number of samples per channel in the audio data.
    size_t n_samples;
    // The audio data itself.
    storage_t *samples;
} sound_t;

// Initialize the given sound to a default state.
void sound_init(sound_t *s);

// Reset the given sound to have the given sample rate and channel setup.
void sound_reset(sound_t *s, size_t sample_rate, size_t n_channels);

// Release the memory held by the given sound.
void sound_destroy(sound_t *s);

// Load a buffer of samples into the given sound. Note that n_samples is the
// total number of samples in the buffer, not per channel.
void sound_load_samples(sound_t *s, const short *samples, size_t n_samples);

// Calculate the formants for the samples in the given sound. The sound is
// modified in place. Return false if the formants couldn't be calculated for
// some reason and true otherwise.
bool sound_calc_formants(sound_t *s, const formant_opts_t *opts);

// Get the i'th sample in the given channel.
static inline storage_t sound_get_sample(const sound_t *s, size_t chan, size_t i) {
    return s->samples[i * s->n_channels + chan];
}

#endif
