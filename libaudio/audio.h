/*
 * PortAudio Portable Real-Time Audio Library
 * PortAudio API Header File
 * Latest version available at: http://www.portaudio.com
 *
 * Copyright (c) 2014-     Formant Industries, Inc.
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
#include <stdbool.h>
#include <pthread.h>
#include "portaudio.h"
#include "pa_ringbuffer.h"

typedef short audio_sample_t; 

typedef struct record_t{

   // Ring buffer (FIFO) for "communicating" from audio callback 
   PaUtilRingBuffer    rBufFromRT;
   void*               rBufFromRTData;

   PaStream*           stream;          // Audio data stream
   
   // Help write end signal read end to extract data
   bool            wakeup;
   pthread_cond_t  cond;
   pthread_mutex_t mutex;

   // General audio settings
   size_t sample_rate; // Sample rate of the audio data in Hz
   size_t n_channels;  // Number of channels (1 for mono, 2 for stereo, and so on)
   size_t n_samples;   // Number of samples collected at once from input buffer 

} record_t;


//*************RECORD_INIT**********************
// 
//**********************************************

bool record_init(
   record_t*           r,
   size_t              sample_format,
   size_t              sample_rate,
   size_t              n_channels,
   size_t              n_samples
);

//*****************RECORD_START*******************
//
//************************************************
bool record_start(record_t *r);

//****************RECORD_STOP***********************
//
//**************************************************
bool record_stop(record_t *r);

//***************RECORD_READ************
//
//**********************************************
void record_read(record_t *r, audio_sample_t *samples);

//***************RECORD_DESTORY*****************
//
void record_destroy(record_t *r);

#endif
