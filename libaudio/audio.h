/*
 * PortAudio Portable Real-Time Audio Library
 * PortAudio API Header File
 * Latest version available at: http://www.portaudio.com
 *
 * Copyright (c) 2014 Formant Industries, Inc.
 * Copyright (c) 1999-2006 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdlib.h>
#include <pthread.h>

#include "portaudio.h"
#include "formant.h"
#include "pa_ringbuffer.h"

//********RECORDING DATA***********
// Input struct for the callback 
// function. Recorded data will
// be stored in a ringbuffer
// for easy access by the read end.
// Write and read threads will
// communicate with pthread 
// signaling.
//********************************
typedef struct record_t{

   // Ring buffer (FIFO) for "communicating" towards audio callback 
   PaUtilRingBuffer    rBufToRT;
   void*               rBufToRTData;
   // Ring buffer (FIFO) for "communicating" from audio callback 
   PaUtilRingBuffer    rBufFromRT;
   void*               rBufFromRTData;

   PaStream*           stream;          // Audio data stream
   
   // Help write end signal read end to extract data
   bool            wakeup;
   pthread_cond_t  cond;
   pthread_mutex_t mutex;

   // General audio settings
   size_t sample_rate;      // Sample rate of the audio data in Hz
   size_t n_channels;       // Number of channels (1 for mono, 2 for stereo, and so on)
   size_t n_samples; // Number of samples collected at once from input buffer 

} record_t;


//*************RECORD_INIT**********************
// 
//**********************************************

void record_init(
   record_t*           r,
   size_t              sample_format,
   size_t              sample_rate,
   size_t              n_chanels,
   size_t              n_samples
);


//*****************RECORD_START*******************
//
//************************************************
void record_start(record_t *r);

//****************RECORD_STOP***********************
//
//**************************************************
void record_stop(record_t *r);

//***************RECORD_ACCESS_SAMPLES************
//
//**********************************************
void record_access_samples(record_t *r);

//*****************RECORDCALLBACK********************
// This is a reference to the PortAudio callback 
// function. This area executes in a separate thread
// from the main program to work on the actual 
// referencing and transfering of the recorded data
// within the inputbuffer. Here we take the data 
// from the inputbuffer and store the data in our
// ringbuffer, which will further pass the data
// for formant calculations in the main thread.	
// More details on this function and proper use are 
// given at the PortAudio website.
//***************************************************
/*static int recordCallback( 
   const void *inputBuffer, 
   void *outputBuffer,
   unsigned long framesPerBuffer,
   const PaStreamCallbackTimeInfo* timeInfo,
   PaStreamCallbackFlags statusFlags,
   void *userData
);*/
#endif
