SRC = pa_ringbuffer.c audio.c
ifeq ($(OS), Windows_NT)
	SRC += mman.c
endif
OBJ = $(SRC:.c=.o)

ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

all: libaudio.a

libaudio.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $< 

.PHONY: all
