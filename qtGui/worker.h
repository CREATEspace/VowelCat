#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

#include "formant.h"
#include "audio.h"

#include "mainwindow.h"
#include "plotter.h"

// Some audio constants for now.
enum { SAMPLE_RATE = 44100 };
enum { CHANNELS = 2 };
enum { SAMPLES = 5000 };

// Duration in nanoseconds over which to tween tracers.
enum { TWEEN_DURATION = 122863987 };

// Worker that processes formants in a separate thread.
typedef struct {
    // Thread ID.
    pthread_t tid;
    // Whether to keep processing.
    bool run;

    // Relevant structures.
    record_t rec;
    formant_opts_t opts;
    sound_t sound;

    // Window to update.
    MainWindow *window;
    plotter_t *plotter;
} worker_t;

// Initialize the given worker.
void worker_init(worker_t *w, MainWindow *window, plotter_t *plotter);

// Free memory held by the given worker.
void worker_destroy(worker_t *w);

// Start the worker in a new thread.
void worker_start(worker_t *w);

// Stop the worker and wait for it to finish processing.
void worker_stop(worker_t *w);

#endif
