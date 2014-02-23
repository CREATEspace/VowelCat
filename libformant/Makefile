SRC = formant.c processing.c
OBJ = $(SRC:.c=.o)
LIB = libformant.a

ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

ALL_LDFLAGS += $(shell pkg-config --libs libformant.pc)
ALL_LDFLAGS += $(LDFLAGS)

all: $(LIB)
test: test-libformant

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

test-libformant: $(SRC) test/test.c
	$(CC) $(ALL_CFLAGS) -DLIBFORMANT_TEST -I. -Itest -o $@ $^ $(ALL_LDFLAGS)

.PHONY: all test
