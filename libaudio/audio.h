//************************************************************************
//
//***************************************************************************** 

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
#include "portaudio.h"
#include "pa_ringbuffer.h"
//***************************


//***************************
#define RB_MULTIPLIER 2       //Used to calculate ring buffer size      
//***************************

typedef short audio_sample_t;

typedef struct audio_t 
{
   size_t sample_rate;
   size_t n_channels;
   size_t frames_per_buffer;

   PaStream *pstream;
   PaStream *rstream;

   bool wakeup_sig;
   pthread_cond_t  wakeup_cond;
   pthread_mutex_t wakeup_mutex;

   enum {
      SOURCE_DISK = 1 << 0,
   }flags;

   audio_sample_t *prbuf;
   size_t prbuf_size;
   size_t prbuf_offset;

   PaUtilRingBuffer rb;
   void *rb_data;

}audio_t;


bool audio_init(audio_t *a, size_t sample_rate, size_t n_channels, size_t frames_per_buffer);
void audio_destroy(audio_t *a);
void audio_reset(audio_t *a);

void audio_open(audio_t *a, audio_sample_t *m_data, size_t m_size);
bool audio_save(audio_t *a, int fd);

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
//END HEADER
