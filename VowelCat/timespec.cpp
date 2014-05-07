// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <inttypes.h>
#include <time.h>

#include "timespec.h"

void timespec_init(timespec_t *ts) {
#ifdef __MACH__
    clock_serv_t clock_serv;

    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clock_serv);
    clock_get_time(clock_serv, ts);
    mach_port_deallocate(mach_task_self(), clock_serv);
#else
    clock_gettime(CLOCK_REALTIME, ts);
#endif
}

uintmax_t timespec_diff(const timespec_t *before, const timespec_t *after) {
    // One second in nanoseconds.
    enum { NSEC = 1000 * 1000 * 1000 };

    if (after->tv_nsec >= before->tv_nsec) {
        return NSEC * (after->tv_sec - before->tv_sec) +
               after->tv_nsec - before->tv_nsec;
    }

    return NSEC * (after->tv_sec - before->tv_sec - 1) +
           NSEC - before->tv_nsec + after->tv_nsec;
}
