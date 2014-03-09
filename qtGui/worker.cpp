#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "mainwindow.h"
#include "plotter.h"
#include "timespec.h"
#include "worker.h"

void worker_init(worker_t *w, MainWindow *window, plotter_t *plotter) {
    bool ret;

#ifdef NDEBUG
    (void) ret;
#endif

    ret = record_init(&w->rec, SAMPLE_RATE, CHANNELS, SAMPLES);
    assert(ret);

    formant_opts_init(&w->opts);

    ret = formant_opts_process(&w->opts);
    assert(ret);

    sound_init(&w->sound);
    w->window = window;
    w->plotter = plotter;
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
        timespec_init(&after);

        dur = (timespec_diff(&before, &after) + count * dur) / (count + 1);
        count += 1;

        avg = 0;

        for (size_t i = 0; i < w->sound.n_samples; i += NOISE_STRIDE)
            avg += ABS(w->sound.samples[i]);

        if (avg < NOISE_THRESHOLD * NOISE_SAMPLES) {
            plotter_wakeup(w->plotter);
            continue;
        }

        ret = sound_calc_formants(&w->sound, &w->opts);
        assert(ret);

        w->window->handleFormants(&w->sound, dur);
        plotter_wakeup(w->plotter);
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
