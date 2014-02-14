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
    const sample_t *rptr = inputBuffer;  


    (void) outputBuffer; // Prevent unused variable warnings. 
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

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
void record_init( 
   record_t*            r,
   size_t               sample_format,     
   size_t               sample_rate,      
   size_t               n_channels,    
   size_t	        n_samples )
{

   //***Initialize PA internal data structures******
   Pa_Initialize();  
   //*************

   //*****Initialize communication ring buffers******************** 
   unsigned long rb_samples_count = 16384; // Must be a power of 2
   r->rBufToRTData = malloc(sizeof(sample_t) * rb_samples_count);  
   PaUtil_InitializeRingBuffer(&r->rBufToRT, sizeof(sample_t), rb_samples_count, r->rBufToRTData); 
   r->rBufFromRTData = malloc(sizeof(sample_t) * rb_samples_count); 
   PaUtil_InitializeRingBuffer(&r->rBufFromRT, sizeof(sample_t), rb_samples_count, r->rBufFromRTData);
   //**************
   
   //******Initialize input device******* 
   PaStreamParameters inputParameters;
   inputParameters.device = Pa_GetDefaultInputDevice();  
   if (inputParameters.device == paNoDevice) {
      //fprintf(stderr,"Error: No default input device.\n");
      return;
   }
   // Specify recording settings 
   inputParameters.channelCount = n_channels;                    
   inputParameters.sampleFormat = sample_format;
   inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
   inputParameters.hostApiSpecificStreamInfo = NULL;
   //***************

   //********Open the input stream******* 
   PaStream *stream=NULL;
   Pa_OpenStream(
      &stream,
      &inputParameters,
      NULL,              
      sample_rate,
      n_samples,
      paClipOff,          
      recordCallback,
      r);
   //**************

   //******Initialize thread settings******
   r->wakeup = false;
   r->cond   = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
   r->mutex  = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
   //**************

   //*********Store audio settings******
   r->stream = stream;
   r->sample_rate = sample_rate;
   r->n_channels = n_channels;
   r->n_samples = n_samples;
   //**************
}
//END FUNCTION

//************************************START RECORDING FROM MIC*********************************************//
//*
void record_start(record_t *r)
{
   Pa_StartStream(r->stream);  // Start recording stream 
   record_access_samples(r);   // Receive and process recorded samples
   Pa_CloseStream(r->stream);  // Close recording stream 

}
//END FUNCTION

//***********************************ACCESS AND PROCESS RECORDED SAMPLES***********************************//
//*
void record_access_samples(record_t *r)
{
   //**********************
   sound_t s;
   sound_init(&s);
   sound_reset(&s, r->sample_rate, r->n_channels);  
   sound_resize(&s, r->n_samples);

   formant_opts_t opts;
   formant_opts_init(&opts);   
   //*******

   for(;;)
   {
      //****************************
      sound_reset(&s, r->sample_rate, r->n_channels);  
      //*******
      
      //****************************
      pthread_mutex_lock(&r->mutex);
      if(!r->wakeup)
         pthread_cond_wait(&r->cond, &r->mutex);
      r->wakeup = false;
      pthread_mutex_unlock(&r->mutex);
      
      PaUtil_ReadRingBuffer(&r->rBufFromRT, &s.samples[0], r->n_samples); 
      sound_resize(&s, r->n_samples);
      
      sound_calc_formants(&s, &opts); 
      //****** 
      
      //****************************
      for (size_t i = 0; i < s.n_samples; i += 1) {
         float f1 = sound_get_f1(&s, i);
         float f2 = sound_get_f2(&s, i);
        
         // Send formants to Qt
      }
      //******
   }
   sound_destroy(&s);
}
//END FUNCTION

//*******************************TERMINATE ALLOCATED DATA FOR RECORDING**************************************//
//*
void record_stop(record_t *r)
{
   Pa_Terminate();

   //*******************
   if (r->rBufToRTData)
      free(r->rBufToRTData);

   if (r->rBufFromRTData)
      free(r->rBufFromRTData);

   pthread_cond_destroy(&r->cond);
   pthread_mutex_destroy(&r->mutex);
   //**********
}
//END FUNCTION
