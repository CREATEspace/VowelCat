//*************
#ifndef PLAY_H
#define PLAY_H

//***************************
#include <stdlib.h>
#include <stdbool.h>
#include "audio.h"
//***************************

//***************************
typedef struct play_t{

   audio_sample_t *pBufData;
   size_t index;
   size_t size;

   PaStream *stream; 

   size_t sample_rate; 
   size_t n_channels; 
   size_t n_samples;    

} play_t;
//***************************
bool play_init(play_t *p, size_t sample_rate, size_t n_channels, size_t n_samples);
bool play_start(play_t *p, audio_sample_t *samples, size_t index, size_t size);
bool play_stop(play_t *p);
#endif
//END HEADER
