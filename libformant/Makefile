SRC = formant.c processing.c
OBJ = $(SRC:.c=.o)
LIB = libformant.a

ECFLAGS := $(CFLAGS)
override CFLAGS =
ELDFLAGS := $(LDFLAGS)
override LDFLAGS =

override CFLAGS += -std=c11 -Wall -Wextra -pipe
override CFLAGS += $(ECFLAGS)

override LDFLAGS += $(shell pkg-config --libs libformant.pc)
override LDFLAGS += $(ELDFLAGS)

all: $(LIB)
test: test-libformant

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test-libformant: test/test.o $(SRC)
	$(MAKE) CFLAGS='-DLIBFORMANT_TEST -I. -Itest' $(LIB) -B
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L.

clean:
	-rm $(OBJ) $(LIB)

distclean: clean
	-rm $(LIB)

.PHONY: all test clean distclean
