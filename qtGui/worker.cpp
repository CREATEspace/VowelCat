#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

extern "C" {
#include "formant.h"
#include "audio.h"
}

#include "mainwindow.h"
#include "worker.h"

void worker_init(worker_t *w, MainWindow *window) {
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
}

void worker_destroy(worker_t *w) {
    sound_destroy(&w->sound);
    record_destroy(&w->rec);
}

// Start recording and processing formants until told to stop.
static void worker_run(worker_t *w) {
    bool ret;

#ifdef NDEBUG
    (void) ret;
#endif

    ret = record_start(&w->rec);
    assert(ret);

    while (w->run) {
        sound_reset(&w->sound, SAMPLE_RATE, CHANNELS);
        sound_resize(&w->sound, SAMPLES);
        record_read(&w->rec, w->sound.samples);

        ret = sound_calc_formants(&w->sound, &w->opts);
        assert(ret);

        w->window->handleFormants(&w->sound, TWEEN_DURATION);
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
