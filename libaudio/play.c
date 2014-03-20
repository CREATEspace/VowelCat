#include "play.h"

static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
   //****************
   (void) inputBuffer; // Prevent unused variable warnings. 
   (void) timeInfo;
   (void) statusFlags;
   //****************

   //****************
   play_t *p = userData;               
   play_sample_t *wptr = outputBuffer;  
   size_t n_samples = framesPerBuffer * p->n_channels;
   //****************


   //****************
   if((p->size - p->index) < n_samples)
   {
      //Might put a memcpy in here
      p->index = p->size;
      return paComplete;
   }

   memcpy(wptr, &p->pBufData[p->index], (n_samples * sizeof(play_sample_t)));
   p->index += n_samples;

   return paContinue;  
   //****************
}

bool play_init(play_t * p, size_t sample_rate, size_t n_channels) 
{
   //*********************************************
   *p = (play_t) {
      .pBufData = NULL,
      .index = 0,
      .size = 0,
      .n_channels  = n_channels,
   };
   //*************

   
   //******Initialize input device******* 
   PaStreamParameters outparams;
   PaDeviceIndex dev;
   PaTime lat;
   //**************
   dev = Pa_GetDefaultOutputDevice();
   if(dev == paNoDevice) return false;
   lat = Pa_GetDeviceInfo(dev)->defaultLowInputLatency;
   //***************
   outparams = (PaStreamParameters) {
      .device = dev,
      .channelCount = n_channels,
      .sampleFormat = paInt16,
      .suggestedLatency = lat,
      .hostApiSpecificStreamInfo = NULL
   };
   //***************

   //********Open the input stream******* 
   PaStream *stream=NULL;
   if(Pa_OpenStream(
      &stream,
      NULL,
      &outparams,              
      sample_rate,
      paFramesPerBufferUnspecified,
      paClipOff,          
      playCallback,
      p) != paNoError) return false;
   p->stream = stream;
   //**************

   return true;
}

void play_set(play_t *p, play_sample_t *samples, size_t index, size_t size)
{
   p->pBufData = samples;
   p->index = index;
   p->size = size;
}
void play_get(play_t *p, size_t *index)
{
   *index = p->index;
}

bool play_start(play_t *p)
{
   //*****************
   return Pa_StartStream(p->stream) == paNoError;  
   //*****************
}

bool play_stop(play_t *p)
{
   //*****************
   return Pa_StopStream(p->stream) == paNoError;
   //*****************
}
