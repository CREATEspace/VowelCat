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

#include "portaudio.h"
#include "formant.h"

//********RECORDING DATA***********
// Input struct for the callback 
// function. sound_t will store
// the recorded samples and carry
// the data to the formant library
// for formant calculations. 
// formant_opts_t will contain
// the recording and formant 
// specifications. These two 
// structs are detailed further in
// the formant.h file.
//********************************
typedef struct record_t{
   sound_t s;
   formant_opts_t opts;
   size_t sample_rate;
   size_t n_channels;
} record_t;


//*************INITIALIZE_RECORDING*************
// Locate user's microphone in the computer
// and initialize the sound_t and formant_opts_t
// objects for use in storing recorded data and 
// converting such data to formant coordinates.
// Audio settings are specified here.
//**********************************************

bool initialize_recording(
   record_t*            record,
   PaStreamParameters*  inputParameters,
   PaStream**           stream,            //Audio data stream
   size_t               n_channels,        //Number of channels
   size_t               sample_format,     //8, 16, 24 or 32 bit integer formats or 32 bit floating point
   size_t               sample_rate,       //Sample rate of the audio data in Hz.
   unsigned long        frames_per_buffer  //the number of sample frames that PortAudio will request from the callback
);

//****************OPEN_AUDIO_INPUT****************
// Function called within initialized_recording().
// Used to locate and power a user's microphone in
// the computer. Microphone settings are carried 
// over from the specifications entered for 
// initialize_recording().
//************************************************
bool open_audio_input(
   PaStreamParameters   *inputParameters,
   size_t               n_channels,
   size_t               sample_format
);

//*****************START_RECORDING****************
// This function serves as a wrapper function
// for the PortAudio StartStream(). A separate 
// thread will be created to begin the processing
// and storing of the recorded data. 
//************************************************
bool start_recording(   
   PaStream             *stream,           
   int                  record_time        //Specify duration of recording in seconds
);

//****************TERMINATE_RECORDING***************
// This function must be called after recording
// to deallocate the space generated for storing
// recorded data samples in the sound_t object.
// Also, the PortAudio recording streams and buffers
// must also be closed.
//**************************************************
void terminate_recording(record_t *record);

//*****************RECORDCALLBACK********************
// This is a reference to the PortAudio callback 
// function. This area executes in a separate thread
// from the main program to work on the actually 
// referencing and transfering of the recorded data
// within the inputbuffer. Here we take the data 
// from the inputbuffer and store the data in the
// sound_t object for formant calculations within
// the formant library. More details on this function
// and proper use are given at the PortAudio website.
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
