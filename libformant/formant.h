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

#include <stdbool.h>

// How input samples are represented.
typedef short sample_t;
// How samples are stored in the sound structure.
typedef float storage_t;

typedef struct sound_t {
    // Sample rate of the audio data in Hz.
    size_t sample_rate;
    // Number of channels (1 for mono, 2 for stereo, and so on).
    size_t n_channels;
    // The number of samples per channel in the audio data.
    size_t n_samples;
    // The audio data itself.
    storage_t *blocks;
} sound_t;

// Initialize the given sound and fill in its sample rate and number of
// channels.
void sound_init(sound_t *s, size_t sample_rate, size_t n_channels);

// Release the memory held by the given sound.
void sound_destroy(sound_t *s);

// Load a buffer of samples into the given sound.
void sound_load_samples(sound_t *s, const short *samples, size_t n_samples);

// Calculate the formants for the samples in the given sound. The sound is
// modified in place.
bool sound_calc_formants(sound_t *s);

// Get the i'th sample in the given channel.
static inline storage_t sound_get_sample(const sound_t *s, size_t chan, size_t i) {
    return s->blocks[i * s->n_channels + chan];
}

#endif
