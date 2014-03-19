#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "audio.h"
#include "play.h"

enum {SAMPLE_RATE = 11025};
enum {CHANNELS = 2};
enum {SAMPLES = 2048};

int main()
{
   int random;
   char str[80];
   size_t size = 250000L;
   play_sample_t *buf = malloc(sizeof(play_sample_t) * size);

   srand(time(NULL));
   for(size_t i = 0; i < size; i++)
   {
      buf[i] = rand() % 20000;
   }

   play_t p;
   audio_init();
   play_init(&p, SAMPLE_RATE, CHANNELS, SAMPLES);
   printf("Enter something to start\n");
   scanf("%s", str);
   play_start(&p, &buf[0], 0, size);
   printf("Enter something to stop\n");
   scanf("%s", str);
   play_stop(&p);
   printf("Enter something to exit\n");
   scanf("%s", str);
   audio_destroy();

   return 0;
}
