SRC = pa_ringbuffer.c audio.c play.c record.c
OBJ = $(SRC:.c=.o)

ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

all: libaudio.a

libaudio.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

.PHONY: all
