SRC = formant.c processing.c
OBJ = $(SRC:.c=.o)
LIB = libformant.a

ECFLAGS := $(CFLAGS)
CFLAGS =
ELDFLAGS := $(LDFLAGS)
LDFLAGS =

# This is OK because overridden variables are explicitely taken into account.
MAKEOVERRIDES =

CFLAGS += -std=c11 -Wall -Wextra -pipe
CFLAGS += $(ECFLAGS)

LDFLAGS += $(shell pkg-config --libs libformant.pc)
LDFLAGS += $(ELDFLAGS)

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
