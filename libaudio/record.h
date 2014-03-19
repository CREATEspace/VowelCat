#ifndef RECORD_H
#define RECORD_H

//***************************
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include "audio.h"
//***************************

//***************************
#define RB_MULTIPLIER 2       //Used to calculate ring buffer size      
//***************************

//***************************
typedef struct record_t{

   // Ring buffer (FIFO) for "communicating" from audio callback 
   PaUtilRingBuffer    rBufFromRT;
   void*               rBufFromRTData;

   PaStream*           stream; //Audio data stream
   
   // Signaling data for read and write threads 
   bool            wakeup;
   pthread_cond_t  cond;
   pthread_mutex_t mutex;

   // General audio settings
   size_t sample_rate; // Sample rate of the audio data in Hz
   size_t n_channels;  // Number of channels (1 for mono, 2 for stereo, and so on)
   size_t n_samples;   // Number of samples collected at once from input buffer 

} record_t;
//***************************


//*************RECORD_INI**********************
// This functions initialzes all the needed data
// structures for recording. A search for the 
// user's microphone is performed followed by
// an initialization of the ring buffer to store
// the recorded samples. The record stream is opened
// and the record callback function is specified 
//*********************************************
bool record_init(
   record_t*           r,
   size_t              sample_rate,
   size_t              n_channels,
   size_t              n_samples
);

//*************RECORD_START********************
// This function turns on the user's microphone
// and starts the recording stream. A user may
// call this function to resume a stream that
// has been paused by record_stop.
//*********************************************
bool record_start(record_t *r);

//*************RECORD_STOP*********************
// This function stops the recording stream.
// It may be used to pause a recording.
//*********************************************
bool record_stop(record_t *r);

//*************RECORD_READ*********************
// This function retrieves the recorded data 
// from the input ring buffer and transfers the
// samples to the samples data array. 
//*********************************************
void record_read(record_t *r, audio_sample_t *samples);

//*************RECORD_DESTORY******************
// This function deallocates all allocated memory. 
//*********************************************
void record_destroy(record_t *r);

#endif
//END HEADER
