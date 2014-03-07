#ifndef PLOTTER_H
#define PLOTTER_H

#include <pthread.h>

#include "mainwindow.h"

typedef struct {
    pthread_t tid;
    bool run;

    bool wakeup;
    pthread_mutex_t wakeup_lock;
    pthread_cond_t wakeup_sig;

    MainWindow *window;
} plotter_t;

void plotter_init(plotter_t *p, MainWindow *window);

void plotter_destroy(plotter_t *p);

void plotter_start(plotter_t *p);

void plotter_stop(plotter_t *p);

void plotter_wait(plotter_t *p);

void plotter_wakeup(plotter_t *p);

#endif
