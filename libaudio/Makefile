ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

INC_FORMANT   = ../libformant
INC_PORTAUDIO = ../libportaudio/include
INC_HEADER    = -I$(INC_FORMANT) -I$(INC_PORTAUDIO)

all: libaudio.a

libaudio.a: audio.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) $(INC_HEADER) -c -o $@ $<
