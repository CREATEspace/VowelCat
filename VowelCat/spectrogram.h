// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <stdlib.h>

#include <QPainter>
#include <QWidget>

extern "C" {
#include "audio.h"
#include "formant.h"
}

#include "formants.h"
#include "params.h"
#include "util.h"

class Spectrogram: public QWidget {
    Q_OBJECT

private:
    enum { FREQ_START = F1_MIN };
    enum { FREQ_END = F2_MAX };
    enum { FREQ_RANGE = FREQ_END - FREQ_START };
    enum { PIXELS_PER_CHUNK = 2 };

    enum { ALPHA = 255 };
    enum { GREY_MIN = 200 };
    enum { GREY_MAX = 255 };
    enum { GREY_RANGE = GREY_MAX - GREY_MIN };

    // Color for the background.
    enum { BACKGROUND = ALPHA << 24 | GREY_MAX << 16 | GREY_MAX << 8 | GREY_MAX };
    // Color for the position indicator.
    enum { INDICATOR = 0xff000000 };

    enum { RADIUS = 300 };

    audio_t *audio;
    sound_t sound;
    Formants formants;

    // The previous offset of the playback indicator.
    int prev;
    // Where the playback indicator should be drawn.
    int pos;

public:
    explicit Spectrogram(audio_t *audio);
    ~Spectrogram();

public slots:
    void update(size_t offset);
    void reset();

signals:
    void clicked(size_t offset);

protected:
    void mouseReleaseEvent(QMouseEvent *event);

private:
    void paintEvent(QPaintEvent *event);

    void draw_chunk(QPainter *paint, int l, int r, int h);
    void update_pos(size_t offset);

    // Convert a sample offset to a pixel offset.
    static int offset_to_pixel(size_t offset);
    // Convert a pixel offset to a sample offset.
    static size_t pixel_to_offset(int pixel);

    // Create a shade of grey for the given bin amplitude as a fraction of the
    // given max amplitude.
    static uint8_t freq_byte(formant_sample_t dist);
    static QRgb freq_argb(formant_sample_t f, formant_sample_t f1,
                          formant_sample_t f2);
};

#endif
