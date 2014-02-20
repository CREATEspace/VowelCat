SRC = audio.c pa_ringbuffer.c
OBJ = $(SRC:.c=.o)

ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

ALL_LDFLAGS += -lm -lasound -lrt -lpthread -lportaudio
ALL_LDFLAGS += $(LDFLAGS)

all: libaudio.a

libaudio.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

.PHONY: all
