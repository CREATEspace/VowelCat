// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#ifndef TIMESPEC_H
#define TIMESPEC_H

#include <inttypes.h>
#include <time.h>

// OSX doesn't support time.h's clock_gettime, so handle it specially.
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

// Both have tv_sec and tv_nsec fields, but OSX gives its a special name for
// some reason.
#ifdef __MACH__
    typedef mach_timespec_t timespec_t;
#else
    typedef struct timespec timespec_t;
#endif

// Initialize the given timespec with the current high-resolution time.
void timespec_init(timespec_t *ts);

// Calculate the difference in nanoseconds between the given timespecs.
uintmax_t timespec_diff(const timespec_t *before, const timespec_t *after);

#endif
