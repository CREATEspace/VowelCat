#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "audio.h"
#include "play.h"

enum {SAMPLE_RATE = 11025};
enum {CHANNELS = 1};

int main()
{
   size_t index = 0;
   size_t size = 250000;
   play_sample_t *buf = malloc(sizeof(play_sample_t) * size);
   srand(time(NULL));
   for(size_t i = 0; i < size; i++)
   {
      buf[i] = rand() % 20000;
   }

   char str;

   play_t p;
   
   audio_init();
   play_init(&p, SAMPLE_RATE, CHANNELS);

   play_set(&p, &buf[0], index, size);
   play_start(&p);

   printf("Wait for the audio to finish or enter something to stop: ");
   getchar();
   str = getchar();

   play_stop(&p);
   play_get(&p, &index);

   printf("Here is the index: %zu\n", index);

   audio_destroy();

   return 0;
}
