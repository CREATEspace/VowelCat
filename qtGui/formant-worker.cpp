#include <assert.h>
#include <stddef.h>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "formant-worker.h"

FormantWorker::FormantWorker():
    running(true)
{}

void FormantWorker::run() {
    enum { SAMPLE_RATE = 44100 };
    enum { SAMPLES = 5000 };
    enum { CHANNELS = 2 };

    bool ret;

    ret = record_init(&rec, SAMPLE_RATE, CHANNELS, SAMPLES);
    assert(ret);

    formant_opts_init(&opts);
    sound_init(&sound);

    ret = formant_opts_process(&opts);
    assert(ret);

    ret = record_start(&rec);
    assert(ret);

    while (running) {
        sound_reset(&sound, SAMPLE_RATE, CHANNELS);
        sound_resize(&sound, SAMPLES);
        record_read(&rec, sound.samples);
        sound_calc_formants(&sound, &opts);

        for (size_t i = 0; i < sound.n_samples; i += 1) {
            formant_sample_t f1 = sound_get_f1(&sound, i);
            formant_sample_t f2 = sound_get_f2(&sound, i);

            emit newFormant(f2, f1);
        }
    }

    sound_destroy(&sound);
    record_destroy(&rec);
}

void FormantWorker::stop() {
    running = false;
    wait();
}
