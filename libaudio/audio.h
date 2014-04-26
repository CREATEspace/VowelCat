// Copyright 2014 Formant Industries. See the Copying file at the top-level
// directory of this project.

//*************
#ifndef AUDIO_H
#define AUDIO_H

//***************************
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include "portaudio.h"
#include "pa_ringbuffer.h"
//***************************

//***************************
#define RB_MULTIPLIER 2
#define PLAY_FPB_DOWNSIZE 2
//***************************

#define WAV_RIFF 0x52494646
#define WAV_WAVE 0x57415645
#define WAV_FMT_ 0x666d7420
#define WAV_DATA 0x64617461

typedef short audio_sample_t;

typedef struct wav_head
{
   uint32_t chunk_id;
   uint32_t chunk_size;
   uint32_t format;

   uint32_t subchunk1_id;
   uint32_t subchunk1_size;

   uint16_t audio_format;
   uint16_t n_channels;
   uint32_t sample_rate;
   uint32_t byte_rate;

   uint16_t block_align;
   uint16_t bits_per_sample;

   uint32_t subchunk2_id;
   uint32_t subchunk2_size;
} wav_head;

typedef struct audio_t
{
   size_t sample_rate;
   size_t n_channels;
   size_t frames_per_buffer;

   PaStream *pstream;
   PaStream *rstream;

   bool wakeup_sig;
   pthread_cond_t wakeup_cond;
   pthread_mutex_t wakeup_mutex;

   enum {
      SOURCE_DISK = 1 << 0,
   } flags;

   audio_sample_t *prbuf;
   size_t prbuf_size;
   size_t prbuf_offset;
   size_t play_offset;

   PaUtilRingBuffer rb;
   void *rb_data;

   wav_head wav;
} audio_t;

bool audio_init(audio_t *a, size_t sample_rate, size_t n_channels, size_t frames_per_buffer);
void audio_destroy(audio_t *a);
void audio_reset(audio_t *a);

void audio_open(audio_t *a, audio_sample_t *m_data, size_t m_size);
void audio_save(audio_t *a, int fd);

bool audio_resize(audio_t *a, size_t n_samples);

bool audio_play(audio_t *a);
bool audio_record(audio_t *a);
void audio_stop(audio_t *a);

void audio_sig_read(audio_t *a);
void audio_sig_write(audio_t *a);

bool audio_play_read(audio_t *a, audio_sample_t *samples);
bool audio_record_read(audio_t *a, audio_sample_t *samples);
bool audio_listen_read(audio_t *a, audio_sample_t *samples);
void audio_seek(audio_t *a, size_t index);

#endif
