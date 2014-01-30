ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe -O2 -march=native -flto
ALL_CFLAGS += $(CFLAGS)

all: libformant.a

libformant.a: formant.o processing.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<
