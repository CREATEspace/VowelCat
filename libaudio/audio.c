#include "audio.h"

//************************CALLBACK FUNCTION***********************************************************//
//*
static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
  
    record_t *r = userData;               
    const audio_sample_t *rptr = inputBuffer;  


    (void) outputBuffer; // Prevent unused variable warnings. 
    (void) framesPerBuffer;
    (void) timeInfo;
    (void) statusFlags;

    //********Pull samples from input buffer***************************
    PaUtil_WriteRingBuffer(&r->rBufFromRT, &rptr[0], r->n_samples);     
    pthread_mutex_lock(&r->mutex);
    r->wakeup = true;
    pthread_cond_signal(&r->cond);  
    pthread_mutex_unlock(&r->mutex);
    //*******************

    return paContinue; // Keep recording stream active. CLOSED ELSEWHERE 
}
//END FUNCTION

//************************INITIALIZE MIC AND AUDIO SETTINGS**********************************************//
//*
bool record_init( 
   record_t*            r,
   size_t               sample_format,     
   size_t               sample_rate,      
   size_t               n_channels,    
   size_t	        n_samples )
{
   //*********************************************
   *r = (record_t) {
      //***Initialize thread settings****
      .wakeup = false,
      .cond   = PTHREAD_COND_INITIALIZER,
      .mutex  = PTHREAD_MUTEX_INITIALIZER,

      //***Store audio settings******
      .sample_rate = sample_rate,
      .n_channels  = n_channels,
      .n_samples   = n_samples
   };
   //*************

   //***Initialize PA internal data structures******
   if(Pa_Initialize() != paNoError) return false;
   //*************

   //*****Initialize communication ring buffers******************** 
   int rb_size = 1 << (sizeof(n_samples) * CHAR_BIT - __builtin_clz(n_samples * RB_MULTIPLIER));
   r->rBufFromRTData = malloc(sizeof(audio_sample_t) * rb_size); 
   PaUtil_InitializeRingBuffer(&r->rBufFromRT, sizeof(audio_sample_t), rb_size, r->rBufFromRTData);
   //**************
   
   //******Initialize input device******* 
   PaStreamParameters inputParameters;
   inputParameters.device = Pa_GetDefaultInputDevice();  
   if(inputParameters.device == paNoDevice) return false;
   // Specify recording settings 
   inputParameters.channelCount = n_channels;                     
   inputParameters.sampleFormat = sample_format;
   inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
   inputParameters.hostApiSpecificStreamInfo = NULL;
   //***************

   //********Open the input stream******* 
   PaStream *stream=NULL;
   if(Pa_OpenStream(
      &stream,
      &inputParameters,
      NULL,              
      sample_rate,
      n_samples,
      paClipOff,          
      recordCallback,
      r) != paNoError) return false;
   r->stream = stream;
   //**************

   return true;
}
//END FUNCTION

//************************************START RECORDING*****************************************************//
//*
bool record_start(record_t *r)
{
   return Pa_StartStream(r->stream) == paNoError; 
}
//END FUNCTION

//************************************STOP RECORDING******************************************************//
//*
bool record_stop(record_t *r)
{
   return Pa_StopStream(r->stream) == paNoError; 
}
//END FUNCTION

//***********************************READ INPUTBUFFER******************************************************//
//*
void record_read(record_t *r, audio_sample_t *samples)
{
   //***************************
   pthread_mutex_lock(&r->mutex);
   if(!r->wakeup)
      pthread_cond_wait(&r->cond, &r->mutex);
   r->wakeup = false;
   pthread_mutex_unlock(&r->mutex);
   //***************************   
   PaUtil_ReadRingBuffer(&r->rBufFromRT, &samples[0], r->n_samples); 
   //***************************   
}
//END FUNCTION


//*******************************TERMINATE ALLOCATED DATA***************************************************//
//*
void record_destroy(record_t *r)
{
   //*******************
   Pa_Terminate();
   //*******************
   free(r->rBufFromRTData);
   //*******************
   pthread_cond_destroy(&r->cond);
   pthread_mutex_destroy(&r->mutex);
   //*******************
}
//END FUNCTION
