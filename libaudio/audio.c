#include "audio.h"


bool audio_init(void) 
{
   //***Initialize PA internal data structures******
   return Pa_Initialize() == paNoError;
   //*******************
}

void audio_destroy(void)
{
   //*******************
   Pa_Terminate(); 
   //*******************
}
