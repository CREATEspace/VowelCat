// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include "audio.h"

#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif

void audio_sig_read(audio_t *a)
{
   pthread_mutex_lock(&a->wakeup_mutex);
   a->wakeup_sig = true;
   pthread_cond_signal(&a->wakeup_cond);
   pthread_mutex_unlock(&a->wakeup_mutex);
}

void audio_sig_write(audio_t *a)
{
   pthread_mutex_lock(&a->wakeup_mutex);
   if(!a->wakeup_sig)
      pthread_cond_wait(&a->wakeup_cond, &a->wakeup_mutex);
   a->wakeup_sig = false;
   pthread_mutex_unlock(&a->wakeup_mutex);
}

static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData)
{
   (void) inputBuffer;
   (void) timeInfo;
   (void) statusFlags;

   audio_t *a = userData;
   audio_sample_t *wptr = outputBuffer;

   size_t n_samples = min(a->prbuf_size - a->prbuf_offset, framesPerBuffer * a->n_channels);
   PaStreamCallbackResult ret = paContinue;

   if(a->prbuf_size <= a->prbuf_offset){
      a->prbuf_offset = a->prbuf_size;
      ret = paComplete;
   } else {
      memcpy(&wptr[0], &a->prbuf[a->prbuf_offset], n_samples * sizeof(audio_sample_t));
      a->prbuf_offset += n_samples;
   }

   a->play_offset = a->prbuf_offset;
   audio_sig_read(a);

   return ret;
}

static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{

   (void) outputBuffer;
   (void) timeInfo;
   (void) statusFlags;

   audio_t *a = userData;
   const audio_sample_t *rptr = inputBuffer;

   //********Pull samples from input buffer***************************
   PaUtil_WriteRingBuffer(&a->rb, &rptr[0], framesPerBuffer * a->n_channels);

   audio_sig_read(a);

   return paContinue;
}

bool audio_init(audio_t *a, size_t sample_rate, size_t n_channels, size_t frames_per_buffer)
{
   //***Initialize PA internal data structures******
   if(Pa_Initialize() != paNoError)
      return false;

   //******Initialize input device*******
   PaStreamParameters inparams;
   PaDeviceIndex dev;
   PaTime lat;

   dev = Pa_GetDefaultInputDevice();
   if(dev == paNoDevice)
      return false;
   lat = Pa_GetDeviceInfo(dev)->defaultLowInputLatency;

   inparams = (PaStreamParameters) {
      .device = dev,
      .channelCount = n_channels,
      .sampleFormat = paInt16,
      .suggestedLatency = lat,
      .hostApiSpecificStreamInfo = NULL
   };

   //******Initialize output device*******
   PaStreamParameters outparams;
   //**************
   dev = Pa_GetDefaultOutputDevice();
   if(dev == paNoDevice)
      return false;
   lat = Pa_GetDeviceInfo(dev)->defaultLowInputLatency;

   outparams = (PaStreamParameters) {
      .device = dev,
      .channelCount = n_channels,
      .sampleFormat = paInt16,
      .suggestedLatency = lat,
      .hostApiSpecificStreamInfo = NULL
   };

   //********Open play stream*******
   PaStream *pstream=NULL;
   if(Pa_OpenStream(
      &pstream,
      NULL,
      &outparams,
      sample_rate,
      (frames_per_buffer >> PLAY_FPB_DOWNSIZE),
      paClipOff,
      playCallback,
      a) != paNoError) return false; //**************

   //********Open record and listen stream*******
   PaStream *rstream=NULL;
   if(Pa_OpenStream(
      &rstream,
      &inparams,
      NULL,
      sample_rate,
      frames_per_buffer,
      paClipOff,
      recordCallback,
      a) != paNoError) return false;

   //*****Initialize communication ring buffers********************
   PaUtilRingBuffer rb;
   void *rb_data;
   size_t rb_size;

   rb_size = 1 << (sizeof(frames_per_buffer) * CHAR_BIT - __builtin_clz(frames_per_buffer * RB_MULTIPLIER));
   rb_data = malloc(sizeof(audio_sample_t) * rb_size);
   PaUtil_InitializeRingBuffer(&rb, sizeof(audio_sample_t), rb_size, rb_data);

   *a = (audio_t) {
      .sample_rate = sample_rate,
      .n_channels = n_channels,
      .frames_per_buffer = frames_per_buffer,

      .pstream = pstream,
      .rstream = rstream,

      .wakeup_sig = false,
      .wakeup_cond   = PTHREAD_COND_INITIALIZER,
      .wakeup_mutex  = PTHREAD_MUTEX_INITIALIZER,

      .flags = DEFAULT,

      .prbuf = NULL,
      .prbuf_size = 0,
      .prbuf_offset = 0,
      .play_offset = 0,

      .rb = rb,
      .rb_data = rb_data
   };

   a->wav = (wav_head) {
      .chunk_id = WAV_RIFF,
      .format = WAV_WAVE,
      .subchunk1_id = WAV_FMT_,
      .subchunk1_size =  16,   //Size of this subchunk
      .audio_format = 1,       //Linear quantization - Other options avail
      .n_channels = n_channels,
      .sample_rate = sample_rate,
      .byte_rate = sample_rate * n_channels * sizeof(audio_sample_t),
      .block_align = n_channels * sizeof(audio_sample_t),
      .bits_per_sample =  sizeof(audio_sample_t) * CHAR_BIT,
      .subchunk2_id = WAV_DATA
   };
      //.chunk_size            //File size - 8 // don't inclue chunk_id and file_dec
      //.subchunk2_size        //sizeofraw * n_channels

   return true;
}

void audio_destroy(audio_t *a)
{
   Pa_CloseStream(a->pstream);
   Pa_CloseStream(a->rstream);
   Pa_Terminate();

   pthread_cond_destroy(&a->wakeup_cond);
   pthread_mutex_destroy(&a->wakeup_mutex);

   audio_reset(a);
   free(a->rb_data);
}

void audio_reset(audio_t *a)
{
   if(a->flags & SOURCE_DISK)
      munmap(a->prbuf, a->prbuf_size * sizeof(audio_sample_t));
   else if(a->prbuf != NULL)
      free(a->prbuf);

   a->prbuf = NULL;
   a->prbuf_size = 0;
   a->prbuf_offset = 0;
   a->play_offset = 0;
   a->flags &= ~SOURCE_DISK;

   PaUtil_FlushRingBuffer(&a->rb);
}

void audio_open(audio_t *a, audio_sample_t *m_data, size_t m_size)
{
   a->prbuf = m_data;
   a->flags |= SOURCE_DISK;
   a->prbuf_size = m_size / sizeof(audio_sample_t);
   a->prbuf_offset = 0;
}

void audio_save(audio_t *a, int fd)
{

   a->wav.chunk_size = sizeof(wav_head) + a->prbuf_size * sizeof(audio_sample_t);
   a->wav.subchunk2_size = a->prbuf_size * sizeof(audio_sample_t);

   write(fd, &a->wav, sizeof(wav_head));
   write(fd, a->prbuf, a->prbuf_size * sizeof(audio_sample_t));
}

bool audio_resize(audio_t *a, size_t n_samples)
{
   audio_sample_t *new_add;
   new_add = realloc(a->prbuf, (a->prbuf_offset + n_samples) * sizeof(audio_sample_t));
   if(new_add == NULL)
      return false;
   a->prbuf = new_add;
   return true;
}

bool audio_play(audio_t *a)
{
   return Pa_StartStream(a->pstream) == paNoError;
}

bool audio_record(audio_t *a)
{
   return Pa_StartStream(a->rstream) == paNoError;
}

void audio_stop(audio_t *a)
{
   Pa_StopStream(a->pstream);
   Pa_StopStream(a->rstream);
   a->wakeup_sig = false;
}

bool audio_play_read(audio_t *a, audio_sample_t *samples)
{
   size_t offset;
   for(size_t i = 0; i < 1 << PLAY_FPB_DOWNSIZE; i++) {
      if(Pa_IsStreamActive(a->pstream)) {
         audio_sig_write(a);
         if(i < 3 && a->prbuf_size == a->prbuf_offset)
            return false;
      }
   }
   offset = a->play_offset;

   if(a->prbuf_size > offset) {
      memcpy(&samples[0], &a->prbuf[offset - a->frames_per_buffer], a->frames_per_buffer * sizeof(audio_sample_t));
      return true;
   }
   else if(a->prbuf_size == offset) {
      memcpy(&samples[0], &a->prbuf[offset - a->frames_per_buffer], a->frames_per_buffer * sizeof(audio_sample_t));
   }
   return false;
}

bool audio_record_read(audio_t *a, audio_sample_t *samples)
{
   if(Pa_IsStreamActive(a->rstream)) {
      audio_sig_write(a);

      if(!audio_resize(a, a->frames_per_buffer * a->n_channels))
         return false;

      PaUtil_ReadRingBuffer(&a->rb, &a->prbuf[a->prbuf_offset], a->frames_per_buffer * a->n_channels);
      memcpy(&samples[0], &a->prbuf[a->prbuf_offset], a->frames_per_buffer * a->n_channels * sizeof(audio_sample_t));

      a->prbuf_size = a->prbuf_offset + a->frames_per_buffer;
      a->prbuf_offset = a->prbuf_size;

      return true;
   }
   return false;

}

bool audio_listen_read(audio_t *a, audio_sample_t *samples)
{
   if(Pa_IsStreamActive(a->rstream)) {
      audio_sig_write(a);

      PaUtil_ReadRingBuffer(&a->rb, &samples[0], a->frames_per_buffer * a->n_channels);
      return true;
   }
   return false;
}

void audio_seek(audio_t *a, size_t index)
{
   a->prbuf_offset = min(a->prbuf_size, index);
}
