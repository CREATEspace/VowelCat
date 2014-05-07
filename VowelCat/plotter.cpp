// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>

#include <QObject>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "formants.h"
#include "params.h"
#include "plotter.h"
#include "timespec.h"
#include "util.h"

Plotter::Plotter(audio_t *a, sound_t *s, Formants *f) :
    run(false),
    audio(a),
    sound(s),
    formants(f)
{}

void Plotter::listen_run() {
    if (!audio_record(audio))
        abort();

    while(run) {
        formants->reset();

        //***********************
        if(!audio_listen_read(audio, sound->samples))
            break;
        //***********************

        if(!formants->calc())
           continue;

        emit newFormant(formants->f1, formants->f2);
    }
}

void Plotter::record_run() {
    if (!audio_record(audio))
        abort();

    while(run) { //Pa_IsStreamActive does not seem to work quick enough
        formants->reset();

        //***********************
        if(!audio_record_read(audio, sound->samples))
            break;
        //***********************

        emit newSamples(audio->prbuf_offset - audio->samples_per_chunk);

        if(!formants->calc())
            continue;

        emit newFormant(formants->f1, formants->f2);
    }
}

void Plotter::play_run() {
    if (!audio_play(audio))
        abort();

    while(run) { //Pa_IsStreamActive does not seem to work quick enough
        formants->reset();

        //***********************
        if(!audio_play_read(audio, sound->samples))
            break;
        //***********************

        emit newSamples(audio->prbuf_offset - audio->samples_per_chunk);

        if(!formants->calc())
            continue;

        emit newFormant(formants->f1, formants->f2);
    }

    if(run)
        emit pauseSig();
}

static void listen_helper(Plotter *p) {
    p->listen_run();
}

static void record_helper(Plotter *p) {
    p->record_run();
}

static void play_helper(Plotter *p) {
    p->play_run();
}

void Plotter::listen() {
    run = true;

    pthread_create(&tid, NULL, [] (void *data) -> void * {
        listen_helper((Plotter *) data);
        pthread_exit(NULL);
    }, this);
}

void Plotter::record() {
    run = true;

    pthread_create(&tid, NULL, [] (void *data) -> void * {
        record_helper((Plotter *) data);
        pthread_exit(NULL);
    }, this);
}

void Plotter::play() {
    run = true;

    pthread_create(&tid, NULL, [] (void *data) -> void * {
        play_helper((Plotter *) data);
        pthread_exit(NULL);
    }, this);
}

void Plotter::stop() {
    // Prevent the plotter thread from doing any more work.
    run = false;

    // Wakeup the plotter thread in case it's sleeping, then wait for it to
    // finish.
    audio_stop(audio);
    pthread_join(tid, NULL);
}
