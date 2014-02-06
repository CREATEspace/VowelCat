#include <stdio.h>
#include <stdlib.h>

#include "audio.h"


//************************CALLBACK FUNCTION************************************
static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
  
    record_t *record = userData;         /* User data to store samples and calculate formants */
    const sample_t *rptr = inputBuffer;  /* Retrieval of recorded samples */


    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    /* Reinitialization after sound_calc_formants */
    record->s.sample_rate = record->sample_rate;
    record->s.n_channels = record->n_channels;

    /* Process recorded data for formant calculations */
    size_t n_samples = framesPerBuffer / (record->s.n_channels);
    sound_load_samples(&record->s,rptr,n_samples); 
    sound_calc_formants(&record->s, &record->opts);

    
     /*for (size_t i = 0; i < record->s.n_samples; i += 1) {
        for (size_t j = 0; j < record->s.n_channels; j += 1)
            printf("%f\n", sound_get_sample(&record->s, j, i));

        putchar('\n');
    } */
    
    
    return paContinue; /* Keep recording stream active. CLOSED ELSEWHERE */
}
//END FUNCTION

//************************INITIALIZE MIC AND AUDIO SETTINGS*********************
bool initialize_recording( 
   record_t*            record,
   PaStreamParameters*  inputParameters,
   PaStream**           stream,
   size_t               n_channels,    
   size_t               sample_format,     
   size_t               sample_rate,      
   unsigned long        frames_per_buffer )
{
   PaError err = paNoError;

   err = Pa_Initialize(); /* Initialize PA internal data structures */
   if( err != paNoError ) return false;
   
   /* Find computer microphone */
   if(!open_audio_input(inputParameters, n_channels, sample_format)) return false;


   /* Initialize structures for recorded data storage and processing */
   sound_init(&record->s, sample_rate, n_channels);
   record->opts = FORMANT_OPTS_DEFAULT;
   formant_opts_process(&record->opts);
   record->sample_rate = sample_rate;
   record->n_channels = n_channels;


   /* Open the input stream */
   err = Pa_OpenStream(
      stream,
      inputParameters,
      NULL,                  /* outputParameters, */
      sample_rate,
      frames_per_buffer,
      paClipOff,      /* We won't output out of range samples so don't bother clipping them */
      recordCallback,
      record);
   if( err != paNoError ) return false;

   return true;
}
//END FUNCTION

//****************************FIND MICROPHONE********************************************
bool open_audio_input(PaStreamParameters *inputParameters, size_t n_channels, size_t sample_format)
{
   inputParameters->device = Pa_GetDefaultInputDevice(); /* default input device */
   if (inputParameters->device == paNoDevice) {
      fprintf(stderr,"Error: No default input device.\n");
      return false;
   }
 
   /* Specify recording settings */
   inputParameters->channelCount = n_channels;                    
   inputParameters->sampleFormat = sample_format;
   inputParameters->suggestedLatency = Pa_GetDeviceInfo( inputParameters->device )->defaultLowInputLatency;
   inputParameters->hostApiSpecificStreamInfo = NULL;
   return true;
}
//END FUNCTION

//*********************START RECORDING FROM MIC***************************************
bool start_recording(PaStream *stream, int record_time)
{
   PaError err = paNoError;

   if(Pa_StartStream( stream ) != paNoError) return false;  /* Start recording stream */

   printf("\n\n*****START SPEAKING INTO THE MIC*******\n");
   
   while(record_time--) /* Countdown in seconds until termination */
   {
      Pa_Sleep(1000);
      printf("Recording... Seconds Left: %d\n", record_time);

   }
   if(Pa_CloseStream( stream ) != paNoError) return false; /* Close recording stream */

   return true;
}
//END FUNCTION

//*********************TERMINATE ALLOCATED DATA FOR RECORDING********************************
void terminate_recording(record_t *record)
{
   Pa_Terminate();
   sound_destroy(&record->s);
}
//END FUNCTION
