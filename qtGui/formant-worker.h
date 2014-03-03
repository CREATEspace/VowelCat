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
    bool running;

    record_t rec;
    formant_opts_t opts;
    sound_t sound;

signals:
    void newFormants(const sound_t *formants, uint64_t nsec);

public slots:
    void stop();

public:
    FormantWorker();

private:
    void run();
};

#endif
