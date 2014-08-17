ifeq ($(OS), Windows_NT)
	SRC = pa_ringbuffer.c mman.c audio.c 
else
	SRC = pa_ringbuffer.c audio.c
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
