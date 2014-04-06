// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "mainwindow.h"
#include "timespec.h"
#include "plotter.h"

void plotter_init(plotter_t *p, MainWindow *window) {
    bool ret;

#ifdef NDEBUG
    (void) ret;
#endif

    ret = audio_init(&p->aud, SAMPLE_RATE, CHANNELS, SAMPLES);
    assert(ret);

    formant_opts_init(&p->opts);
    p->opts.pre_emph_factor = 1;
    p->opts.downsample_rate = SAMPLE_RATE;
    ret = formant_opts_process(&p->opts);
    assert(ret);

    sound_init(&p->sound);
    p->window = window;
}

void plotter_destroy(plotter_t *p) {
    sound_destroy(&p->sound);
    audio_destroy(&p->aud);
}

#define ABS(x) ((x) > 0 ? (x) : -(x))

// Start recording and processing formants until told to stop.
static void plotter_run(plotter_t *p) {
    enum { NOISE_STRIDE = SAMPLES / NOISE_SAMPLES };

    bool ret;
    uintmax_t avg;
    uintmax_t f1, f2;

    timespec_t before, after;
    uintmax_t dur = 0, count = 0;

#ifdef NDEBUG
    (void) ret;
#endif

    ret = audio_record(&p->aud);
    assert(ret);

    while (p->run) {
        timespec_init(&before);
        sound_reset(&p->sound, SAMPLE_RATE, CHANNELS);
        sound_resize(&p->sound, SAMPLES);
        audio_listen_read(&p->aud, p->sound.samples);

        avg = 0;

        for (size_t i = 0; i < p->sound.n_samples; i += NOISE_STRIDE)
            avg += ABS(p->sound.samples[i]);

        if (avg < NOISE_THRESHOLD * NOISE_SAMPLES)
            continue;

        ret = sound_calc_formants(&p->sound, &p->opts);
        assert(ret);

        f1 = 0;

        for (size_t i = 0; i < p->sound.n_samples; i += 1)
            f1 += sound_get_f1(&p->sound, i);

        f2 = 0;

        for (size_t i = 0; i < p->sound.n_samples; i += 1)
            f2 += sound_get_f2(&p->sound, i);

        f1 /= p->sound.n_samples;
        f2 /= p->sound.n_samples;

        timespec_init(&after);

        dur = (timespec_diff(&before, &after) + count * dur) / (count + 1);
        count += 1;

        p->window->plotFormant(f1, f2, dur);
    }

    audio_stop(&p->aud);
}

void plotter_start(plotter_t *p) {
    p->run = true;

    pthread_create(&p->tid, NULL, [] (void *data) -> void * {
        plotter_run((plotter_t *) data);
        pthread_exit(NULL);
    }, p);
}

void plotter_stop(plotter_t *p) {
    p->run = false;
    pthread_join(p->tid, NULL);
}
