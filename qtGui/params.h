// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#ifndef PARAMS_H
#define PARAMS_H

static const size_t SAMPLE_RATE = 10000;
static const size_t CHANNELS = 1;
static const size_t FRAMES_PER_CHUNK = 1000;
static const size_t SAMPLES_PER_CHUNK = FRAMES_PER_CHUNK * CHANNELS;

static const uint32_t F1_MIN = 150;
static const uint32_t F1_MAX = 850;

static const uint32_t F2_MIN = 700;
static const uint32_t F2_MAX = 2350;

#endif
