#include "greatest.h"

extern SUITE(formant_suite);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    GREATEST_RUN_SUITE(formant_suite);
    GREATEST_MAIN_END();
}
