// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

#include <stdint.h>

#include "audio.h"

//***************************
#define RB_MULTIPLIER 2
//***************************

#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif

typedef struct {
   uint8_t chunk_id[4];
   uint32_t chunk_size;
   uint8_t format[4];

   uint8_t subchunk1_id[4];
   uint32_t subchunk1_size;

   uint16_t audio_format;
   uint16_t n_channels;
   uint32_t sample_rate;
   uint32_t byte_rate;

   uint16_t block_align;
   uint16_t bits_per_sample;

   uint8_t subchunk2_id[4];
   uint32_t subchunk2_size;
} wav_header_t;

void wav_header_init(wav_header_t *h, const audio_t *a) {
   enum { CHUNK_SIZE = sizeof(*h) - sizeof(h->chunk_id) - sizeof(h->chunk_size) };

   *h = (wav_header_t) {
      .chunk_id = {'R', 'I', 'F', 'F'},
      .chunk_size = CHUNK_SIZE + a->prbuf_size * a->n_channels *
         sizeof(audio_sample_t),
      .format = {'W', 'A', 'V', 'E'},

      .subchunk1_id = {'f', 'm', 't', ' '},
      .subchunk1_size = 16,

      .audio_format = a->n_channels,
      .n_channels = a->n_channels,
      .sample_rate = a->sample_rate,
      .byte_rate = a->sample_rate * a->n_channels * sizeof(audio_sample_t),

      .block_align = a->n_channels * sizeof(audio_sample_t),
      .bits_per_sample = sizeof(audio_sample_t) * CHAR_BIT,

      .subchunk2_id = {'d', 'a', 't', 'a'},
      .subchunk2_size = a->prbuf_size * a->n_channels * sizeof(audio_sample_t),
   };
}

void audio_wakeup(audio_t *a)
{
   pthread_mutex_lock(&a->wakeup_mutex);
   a->wakeup = true;
   pthread_cond_signal(&a->wakeup_sig);
   pthread_mutex_unlock(&a->wakeup_mutex);
}

void audio_wait(audio_t *a)
{
   pthread_mutex_lock(&a->wakeup_mutex);
   if(!a->wakeup)
      pthread_cond_wait(&a->wakeup_sig, &a->wakeup_mutex);
   a->wakeup = false;
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

   if(a->prbuf_offset == a->prbuf_size) {
      audio_wakeup(a);
      return paComplete;
   }

   memcpy(&wptr[0], &a->prbuf[a->prbuf_offset], n_samples * sizeof(audio_sample_t));
   a->prbuf_offset += n_samples;

   audio_wakeup(a);

   return paContinue;
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

   audio_wakeup(a);

   return paContinue;
}

bool audio_init(audio_t *a, size_t sample_rate, size_t n_channels, size_t samples_per_chunk)
{
   //***Initialize PA internal data structures******
   if(Pa_Initialize() != paNoError)
      return false;

   size_t frames_per_buffer = samples_per_chunk / n_channels;

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
      frames_per_buffer,
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

   rb_size = 1 << (sizeof(samples_per_chunk) * CHAR_BIT - __builtin_clz(samples_per_chunk * RB_MULTIPLIER));
   rb_data = malloc(sizeof(audio_sample_t) * rb_size);
   PaUtil_InitializeRingBuffer(&rb, sizeof(audio_sample_t), rb_size, rb_data);

   *a = (audio_t) {
      .sample_rate = sample_rate,
      .n_channels = n_channels,
      .samples_per_chunk = samples_per_chunk,

      .pstream = pstream,
      .rstream = rstream,

      .wakeup = false,
      .wakeup_sig   = PTHREAD_COND_INITIALIZER,
      .wakeup_mutex  = PTHREAD_MUTEX_INITIALIZER,

      .flags = DEFAULT,

      .prbuf = NULL,
      .prbuf_size = 0,
      .prbuf_offset = 0,

      .rb = rb,
      .rb_data = rb_data
   };

   return true;
}

void audio_destroy(audio_t *a)
{
   Pa_CloseStream(a->pstream);
   Pa_CloseStream(a->rstream);
   Pa_Terminate();

   pthread_cond_destroy(&a->wakeup_sig);
   pthread_mutex_destroy(&a->wakeup_mutex);

   audio_clear(a);
   free(a->rb_data);
}

void audio_reset(audio_t *a)
{
   // Clear the wakeup flag in case it was set by audio_stop but no thread was
   // sleeping and so didn't clear it.
   a->wakeup = false;
   PaUtil_FlushRingBuffer(&a->rb);
}

void audio_clear(audio_t *a)
{
   if(a->flags & SOURCE_DISK)
      munmap(a->prbuf, a->prbuf_size * sizeof(audio_sample_t));
   else
      free(a->prbuf);

   a->prbuf = NULL;
   a->prbuf_size = 0;
   a->prbuf_offset = 0;
   a->flags &= ~SOURCE_DISK;

   audio_reset(a);
}

void audio_open(audio_t *a, FILE *fp)
{
   int fd;
   struct stat st;
   size_t m_size;
   audio_sample_t *m_data;
   
   fd = fileno(fp);
   fstat(fd, &st);
   m_size = st.st_size;
   
   m_data = (audio_sample_t*) mmap(NULL, m_size, PROT_READ, MAP_SHARED, fd, 0);

   // HACK: skip over the wav header, and come up with a better solution soon.
   a->prbuf = (audio_sample_t *)((uint8_t *) m_data + sizeof(wav_header_t));
   a->flags |= SOURCE_DISK;
   a->prbuf_size = m_size / sizeof(audio_sample_t);
   a->prbuf_offset = 0;
}

void audio_save(audio_t *a, FILE *fp)
{
   wav_header_t header;
   wav_header_init(&header, a);

   fwrite(&header, sizeof(wav_header_t), 1, fp);
   fwrite(a->prbuf, sizeof(audio_sample_t), a->prbuf_size, fp);
}

// Grow the given audio buffer so it can hold the given additional number of
// samples.
static bool audio_grow(audio_t *a, size_t n_samples)
{
   audio_sample_t *new_add;
   new_add = realloc(a->prbuf, (a->prbuf_size + n_samples) * sizeof(audio_sample_t));
   if(new_add == NULL)
      return false;
   a->prbuf = new_add;
   a->prbuf_size += n_samples;
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

   // Wakeup any waiting threads so they can acknowledge the stopped streams.
   audio_wakeup(a);
}

bool audio_play_read(audio_t *a, audio_sample_t *samples)
{
   // Copy here in case prbuf_offset is modified by the play thread while this
   // function is running.
   size_t offset = a->prbuf_offset;

   // If there aren't enough samples left to make up a chunk, bail.
   if (a->prbuf_size - offset < a->samples_per_chunk)
      return false;

   if (!Pa_IsStreamActive(a->pstream))
      return false;

   audio_wait(a);

   // If we get here, then there are enough samples to make up a chunk, and the
   // user hasn't stopped playback, so copy out the samples for this chunk.
   memcpy(&samples[0], &a->prbuf[offset], a->samples_per_chunk * sizeof(audio_sample_t));

   return true;
}

bool audio_record_read(audio_t *a, audio_sample_t *samples)
{
   if(!Pa_IsStreamActive(a->rstream))
      return false;

   audio_wait(a);

   if(!audio_grow(a, a->samples_per_chunk))
      return false;

   PaUtil_ReadRingBuffer(&a->rb, &a->prbuf[a->prbuf_offset], a->samples_per_chunk);
   memcpy(&samples[0], &a->prbuf[a->prbuf_offset], a->samples_per_chunk * sizeof(audio_sample_t));

   a->prbuf_offset = a->prbuf_size;

   return true;
}

bool audio_listen_read(audio_t *a, audio_sample_t *samples)
{
   if (!Pa_IsStreamActive(a->rstream))
      return false;

   audio_wait(a);
   PaUtil_ReadRingBuffer(&a->rb, &samples[0], a->samples_per_chunk);

   return true;
}

void audio_seek(audio_t *a, size_t index)
{
   a->prbuf_offset = min(a->prbuf_size, index);
}
