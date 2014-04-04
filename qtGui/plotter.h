// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#ifndef PLOTTER_H
#define PLOTTER_H

#include <pthread.h>

#include "audio.h"
#include "record.h"
#include "play.h"
#include "formant.h"
#include "mainwindow.h"

// Some audio constants for now.
enum { SAMPLE_RATE = 11025 };
enum { CHANNELS = 2 };
enum { SAMPLES = 2048 };

// Number of samples to take into account when checking for noise.
enum { NOISE_SAMPLES = SAMPLES / 500 };
// If recorded samples have an average value less than this, then consider them
// noise.
enum { NOISE_THRESHOLD = 80 };

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
} plotter_t;

// Initialize the given plotter.
void plotter_init(plotter_t *p, MainWindow *window);

// Free memory held by the given plotter.
void plotter_destroy(plotter_t *p);

// Start the plotter in a new thread.
void plotter_start(plotter_t *p);

// Stop the plotter and wait for it to finish processing.
void plotter_stop(plotter_t *p);

#endif
