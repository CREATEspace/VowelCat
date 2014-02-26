#ifndef FORMANT_WORKER_H
#define FORMANT_WORKER_H

#include <QThread>
#include <QObject>

extern "C" {
#include "audio.h"
#include "formant.h"
}

class FormantWorker: public QThread {
    Q_OBJECT

private:
    record_t rec;
    formant_opts_t opts;
    sound_t sound;

signals:
    void newFormant(formant_sample_t f2, formant_sample_t f1);

public:
    ~FormantWorker();

private:
    void run();
};

#endif
