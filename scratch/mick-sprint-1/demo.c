// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "formant.h"

enum { X_MIN = 500 };
enum { X_MAX = 2500 };
enum { X_RANGE = X_MAX - X_MIN };
enum { COLS = 100 };
enum { POINTS_PER_COL = X_RANGE / COLS };

enum { Y_MIN = 200 };
enum { Y_MAX = 900 };
enum { Y_RANGE = Y_MAX - Y_MIN };
enum { ROWS = 35 };
enum { POINTS_PER_ROW = Y_RANGE / ROWS };

static char EMPTY[COLS + 1];

static void draw_plot(size_t x, size_t y) {
    if (x < X_MIN || x > X_MAX || y < Y_MIN || y > Y_MAX)
        return;

    size_t row = ROWS * (y - Y_MIN) / (Y_RANGE + 1);
    size_t col = COLS - (COLS * (x - X_MIN) / (X_RANGE + 1)) - 1;
    size_t c, r;

    printf("\x1b[2m");

    for (r = 0; r < row; r += 1) {
        if (r & 1)
            printf(" %3zu \u2502%s\n", Y_MIN + r * POINTS_PER_ROW, EMPTY);
        else
            printf("     \u2502%s\n", EMPTY);
    }

    if (r & 1)
        printf(" %3zu \u2502", Y_MIN + r * POINTS_PER_ROW);
    else
        printf("     \u2502");

    for (c = 0; c < col; c += 1)
        putchar('.');

    printf("\x1b[36;46;1m\u2588\x1b[0m");

    for (c += 1; c < COLS; c += 1)
        putchar('.');

    putchar('\n');

    for (r += 1; r < ROWS; r += 1) {
        if (r & 1)
            printf(" %3zu \u2502%s\n", Y_MIN + r * POINTS_PER_ROW, EMPTY);
        else
            printf("     \u2502%s\n", EMPTY);
    }

    printf("     \u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n"
           "      2500      2250       2000        1750         1500         1250          1000         750        500\n"
           "\n"
           "                                                (%-4zu, %3zu)\n"
           ,
           x, y);
}

int main(int argc, char **argv) {
    enum { N_SAMPLES = 5000 };
    enum { N_CHANNELS = 1 };
    enum { SAMPLE_RATE = 16000 };

    int fd;
    struct stat st;
    formant_sample_t *mem;

    for (size_t i = 0; i < COLS; i += 1)
        EMPTY[i] = '.';
    EMPTY[COLS] = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s RAW-AUDIO-FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd = open(argv[1], O_RDONLY);
    fstat(fd, &st);

    mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    size_t n_samples = st.st_size / sizeof(formant_sample_t);

    formant_opts_t opts;
    formant_opts_init(&opts);
    opts.n_formants = 2;
    formant_opts_process(&opts);

    sound_t sound;
    sound_init(&sound);

    system("tput civis");

    unsigned long long sumx, sumy;
    unsigned long long n;

    sumx = sumy = n = 0;

    for (size_t s = 0; n_samples - s >= N_SAMPLES; s += N_SAMPLES) {
        sound_reset(&sound, SAMPLE_RATE, N_CHANNELS);
        sound_load_samples(&sound, &mem[s], N_SAMPLES);
        sound_calc_formants(&sound, &opts);

        for (size_t i = 0; i < sound.n_samples; i += 1) {
            formant_sample_t f1 = sound_get_sample(&sound, 0, i);
            formant_sample_t f2 = sound_get_sample(&sound, 1, i);

            printf("%hd\n%hd\n", f1, f2);
            putchar('\n');

            sumx += f2;
            sumy += f1;
        }

        n += sound.n_samples;
    }

    system("tput cnorm");

    sound_destroy(&sound);
}

