ALL_CFLAGS += -std=c11 -Wall -Wextra -pipe
ALL_CFLAGS += $(CFLAGS)

INC_FORMANT   = ../libformant
INC_PORTAUDIO = ../libportaudio/include
INC_HEADER    = -I$(INC_FORMANT) -I$(INC_PORTAUDIO)

LINK_PORTAUDIO = ../build/libportaudio.a
LINK_FORMANT   = ../build/libformant.a
LINK_STATIC    = -L$(LINK_PORTAUDIO) -L$(LINK_FORMANT)

all: libaudio.a

libaudio.a: audio.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(ALL_CFLAGS) $(INC_HEADER) -c -o $@ $< $(LINK_STATIC) 
