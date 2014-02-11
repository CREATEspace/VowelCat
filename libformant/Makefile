SRC = formant.c processing.c
OBJ = $(SRC:.c=.o)

ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

ALL_LDFLAGS += -lm
ALL_LDFLAGS += $(LDFLAGS)

all: libformant.a
test: test-libformant

libformant.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

test-libformant: $(SRC) test/test.c
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) -DLIBFORMANT_TEST -I. -Itest -o $@ $^

.PHONY: all test
