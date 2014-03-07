#include <pthread.h>
#include <stdlib.h>

#include "plotter.h"
#include "mainwindow.h"

void plotter_init(plotter_t *p, MainWindow *window) {
    p->wakeup = false;
    p->wakeup_lock = PTHREAD_MUTEX_INITIALIZER;
    p->wakeup_sig = PTHREAD_COND_INITIALIZER;
    p->window = window;
}

void plotter_destroy(plotter_t *p) {
    pthread_mutex_destroy(&p->wakeup_lock);
    pthread_cond_destroy(&p->wakeup_sig);
}

static void plotter_run(plotter_t *p) {
    while (p->run)
        if (!p->window->plotFormant())
            plotter_wait(p);
}

void plotter_start(plotter_t *p) {
    p->run = true;

    pthread_create(&p->tid, NULL, [] (void *data) -> void * {
        plotter_run((plotter_t *) data);
        pthread_exit(NULL);
    }, p);
}

void plotter_stop(plotter_t *p) {
    p->run = false;
    plotter_wakeup(p);
    pthread_join(p->tid, NULL);
}

void plotter_wait(plotter_t *p) {
    pthread_mutex_lock(&p->wakeup_lock);

    if (!p->wakeup)
        pthread_cond_wait(&p->wakeup_sig, &p->wakeup_lock);

    pthread_mutex_unlock(&p->wakeup_lock);
}

void plotter_wakeup(plotter_t *p) {
    pthread_mutex_lock(&p->wakeup_lock);
    p->wakeup = true;
    pthread_cond_signal(&p->wakeup_sig);
    pthread_mutex_unlock(&p->wakeup_lock);
}
