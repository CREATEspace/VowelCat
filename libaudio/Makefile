ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

all: libaudio.a

libaudio.a: audio.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c -o $@ $<
