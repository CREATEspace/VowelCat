#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stddef.h>

#define FORMANT_SAMPLE_RATE 10000

#define FORMANT_COUNT 4
#define MAX_FORMANTS 7

#define LPC_STABLE 70
#define LPC_ORDER 12
#define LPC_ORDER_MIN 2
#define LPC_ORDER_MAX 30
#define LPC_COEF (LPC_ORDER + 1)

// Nominal F1 frequency.
#define NOM_FREQ 0

static_assert(LPC_ORDER <= LPC_ORDER_MAX && LPC_ORDER >= LPC_ORDER_MIN,
    "LPC_ORDER out of bounds");

static_assert(FORMANT_COUNT <= MAX_FORMANTS,
    "FORMANT_COUNT out of bounds");

static_assert(FORMANT_COUNT <= (LPC_ORDER - 4) / 2,
    "FORMANT_COUNT too large");

#endif
