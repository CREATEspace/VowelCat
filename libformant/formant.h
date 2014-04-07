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

typedef struct {
    formant_sample_t f1, f2;
} formants_t;

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

// Calculate the formants for the samples in the given sound.
void formants_calc(formants_t *f, const sound_t *s);

#endif
