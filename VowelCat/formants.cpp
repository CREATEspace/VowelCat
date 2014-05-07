// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "formants.h"
#include "params.h"

Formants::Formants(audio_t *a, sound_t *s):
    audio(a),
    sound(s)
{
    formant_opts_init(&opts);
    opts.pre_emph_factor = 1;
    opts.window_type = WINDOW_TYPE_HAMMING;

    if (!formant_opts_process(&opts))
        abort();
}

#define ABS(x) ((x) > 0 ? (x) : -(x))

bool Formants::is_noise() {
    enum { NOISE_STRIDE = SAMPLES_PER_CHUNK / NOISE_SAMPLES };
    uintmax_t avg = 0;

    for (size_t i = 0; i < sound->n_samples; i += NOISE_STRIDE)
        avg += ABS(sound->samples[i]);

    return avg < NOISE_THRESHOLD * NOISE_SAMPLES;
}

bool Formants::calc() {
    if (is_noise())
        return false;

    if (!sound_calc_formants(sound, &opts))
        abort();

    f1 = 0;

    for (size_t i = 0; i < sound->n_samples; i += 1)
        f1 += sound_get_f1(sound, i);

    f2 = 0;

    for (size_t i = 0; i < sound->n_samples; i += 1)
        f2 += sound_get_f2(sound, i);

    f1 /= sound->n_samples;
    f2 /= sound->n_samples;

    return f1 >= F1_MIN && f1 <= F1_MAX && f2 >= F2_MIN && f2 <= F2_MAX;
}

bool Formants::calc(size_t offset) {
    reset();

    memcpy(sound->samples, &audio->prbuf[offset], audio->samples_per_chunk *
        sizeof(audio_sample_t));

    return calc();
}

void Formants::reset() {
    sound_reset(sound, SAMPLE_RATE, CHANNELS);
    sound_resize(sound, SAMPLES_PER_CHUNK);
}
