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

test-libformant: test/test.o $(SRC)
	$(MAKE) CFLAGS='-DLIBFORMANT_TEST -I. -Itest' $(LIB) -B
	$(CC) $(ALL_CFLAGS) -o $@ $< $(ALL_LDFLAGS) -L.

clean:
	-rm $(OBJ) $(LIB)

.PHONY: all test clean
