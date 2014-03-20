//*************
#ifndef PLAY_H
#define PLAY_H

//***************************
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "portaudio.h"
//***************************

typedef short play_sample_t;

//***************************
typedef struct play_t{
   play_sample_t *pBufData;
   size_t index;
   size_t size;

   PaStream *stream; 

   size_t n_channels; 
} play_t;
//***************************
bool play_init(play_t *p, size_t sample_rate, size_t n_channels);
void play_set(play_t *p, play_sample_t *samples, size_t index, size_t size);
void play_get(play_t *p, size_t *index);
bool play_start(play_t *p);
bool play_stop(play_t *p);
#endif
//END HEADER
