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
#include "worker.h"

void worker_init(worker_t *w, MainWindow *window) {
    bool ret;

#ifdef NDEBUG
    (void) ret;
#endif

    ret = record_init(&w->rec, SAMPLE_RATE, CHANNELS, SAMPLES);
    assert(ret);

    formant_opts_init(&w->opts);
    w->opts.pre_emph_factor = 1;
    w->opts.downsample_rate = SAMPLE_RATE;
    ret = formant_opts_process(&w->opts);
    assert(ret);

    sound_init(&w->sound);
    w->window = window;
}

void worker_destroy(worker_t *w) {
    sound_destroy(&w->sound);
    record_destroy(&w->rec);
}

#define ABS(x) ((x) > 0 ? (x) : -(x))

// Start recording and processing formants until told to stop.
static void worker_run(worker_t *w) {
    enum { NOISE_STRIDE = SAMPLES / NOISE_SAMPLES };

    bool ret;
    uintmax_t avg;
    uintmax_t f1, f2;

    timespec_t before, after;
    uintmax_t dur = 0, count = 0;

#ifdef NDEBUG
    (void) ret;
#endif

    ret = record_start(&w->rec);
    assert(ret);

    while (w->run) {
        timespec_init(&before);
        sound_reset(&w->sound, SAMPLE_RATE, CHANNELS);
        sound_resize(&w->sound, SAMPLES);
        record_read(&w->rec, w->sound.samples);

        avg = 0;

        for (size_t i = 0; i < w->sound.n_samples; i += NOISE_STRIDE)
            avg += ABS(w->sound.samples[i]);

        if (avg < NOISE_THRESHOLD * NOISE_SAMPLES)
            continue;

        ret = sound_calc_formants(&w->sound, &w->opts);
        assert(ret);

        f1 = 0;

        for (size_t i = 0; i < w->sound.n_samples; i += 1)
            f1 += sound_get_f1(&w->sound, i);

        f2 = 0;

        for (size_t i = 0; i < w->sound.n_samples; i += 1)
            f2 += sound_get_f2(&w->sound, i);

        f1 /= w->sound.n_samples;
        f2 /= w->sound.n_samples;

        timespec_init(&after);

        dur = (timespec_diff(&before, &after) + count * dur) / (count + 1);
        count += 1;

        w->window->plotFormant(f1, f2, dur);
    }

    record_stop(&w->rec);
}

void worker_start(worker_t *w) {
    w->run = true;

    pthread_create(&w->tid, NULL, [] (void *data) -> void * {
        worker_run((worker_t *) data);
        pthread_exit(NULL);
    }, w);
}

void worker_stop(worker_t *w) {
    w->run = false;
    pthread_join(w->tid, NULL);
}
