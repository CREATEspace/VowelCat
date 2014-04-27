// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#ifndef FORMANTS_H
#define FORMANTS_H

#include <assert.h>
#include <inttypes.h>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "params.h"

class Formants {
private:
    audio_t *audio;
    sound_t *sound;
    formant_opts_t opts;

    // Number of samples to take into account when checking for noise.
    static const uint32_t NOISE_SAMPLES = 4;
    // If recorded samples have an average value less than this, then consider
    // them noise.
    static const uint32_t NOISE_THRESHOLD = 100;

    static_assert(NOISE_SAMPLES > 0 && NOISE_SAMPLES < SAMPLES_PER_CHUNK,
                  "NOISE_SAMPLES is within bounds");

public:
    // Store these as unsigned since they are calculated as averages and may
    // become large before the final division.
    uintmax_t f1, f2;

    Formants(audio_t *a, sound_t *s);

    void reset();
    bool calc();

    // Get the F1 and F2 values at the given sample offset. Return true if the
    // chunk is valid audio and false if it's noise.
    bool calc(size_t offset);

private:
    bool is_noise();
};

#endif
