#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#define DEVICE_NAME "default"


/**********************************************************************/
typedef struct ADesc {

#ifdef HPUX
  Audio    *audio;
  ATransID  transid;
  int       Socket;
  int       last;
  int       convert;
  double    time;
  int       freq;
#endif

#ifdef OSS
  int    afd;
  /*int    count;*/
  int    frag_size;
  double time;
  double timep;
  int    freq;
  int    convert;
  int    warm;
#endif

  snd_pcm_t *handle;
  int       freq;
  long      nWritten;
  long      nPlayed;

#ifdef Solaris
  int afd;
  audio_info_t ainfo;
  double time;
  double timep;
  int freq;
  int convert;
  short *convBuf;
  int convSize;
#endif

#ifdef WIN
  int curr;
  int freq;
  int shortRead;
  int convert;

  PCMWAVEFORMAT pcmwf;
  DSBUFFERDESC dsbdesc;
  DSCBUFFERDESC dscbdesc;
  LPDIRECTSOUNDBUFFER lplpDsb;
  LPDIRECTSOUNDCAPTUREBUFFER lplpDscb;
  PCMWAVEFORMAT pcmwfPB;
  DSBUFFERDESC dsbdescPB;
  LPDIRECTSOUNDBUFFER lplpDsPB;
  unsigned int BufPos;
  int BufLen;
  long written;
  long lastWritten;
#endif

#ifdef IRIX
  ALport   port;
  ALconfig config;
  unsigned long long startfn;
  int count;
#endif

#if defined(MAC)/* || defined(OS_X_CORE_AUDIO)*/
  /* Fields for handling output */
  SndChannelPtr schn;
  SndCommand	  scmd;
  SndDoubleBufferHeader2  dbh;
  SndDoubleBufferPtr	   bufs[NBUFS]; /* the two double buffers */
  int currentBuf;	/* our own track of which buf is current */
  int bufsIssued;	/* For record: how many bufs have been set going */
  int bufsCompleted;	/* For record: how many bufs have completed */
  int bufFull[NBUFS];
  long     bufFrames;	    /* number of frames allocated per buffer */
  int running;	/* flag as to whether we have started yet */
  int pause;    /* flag that we are paused (used on input only?) */
  /* data for the callbacks */
  void     *data;	    /* pointer to the base of the sampled data */
  long     totalFrames;   /* how many frames there are */
  long     doneFrames;    /* how many we have already copied */
  /* Fields for input */
  long inRefNum;	    /* MacOS reference to input channel */
  SPBPtr spb[NBUFS];	    /* ptr to the parameter blocks for recording */
  /* debug stats */
  int completedblocks;
  int underruns;
#endif /* MAC */

#ifdef OS_X_CORE_AUDIO
  AudioDeviceID	device;
  UInt32 deviceBufferSize;
  AudioStreamBasicDescription deviceFormat;
  int rpos, wpos;
  double time;
  int tot;
  int encoding;
#endif /* OS_X_CORE_AUDIO */

  int bytesPerSample;
  int nChannels;
  int mode;
  int debug;

} ADesc;
/**********************************************************/

#define RECORD 1
#define PLAY   2

#define SNACK_MONO   1
#define SNACK_STEREO 2
#define SNACK_QUAD   4

#define LIN16        1
#define ALAW         2
#define MULAW        3
#define LIN8OFFSET   4
#define LIN8         5
#define LIN24        6
#define LIN32        7
#define SNACK_FLOAT  8
#define SNACK_DOUBLE 9
#define LIN24PACKED 10

#define CAPABLEN 100


static char *defaultDeviceName = DEVICE_NAME;

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

static int mfd = 0;
static int littleEndian = 0;
static int minNumChan = 1;
static ADesc ado;
char defaultInDevice[100];
static int globalRate = 16000;

/******************OPEN MIC*********************************/
void SnackAudioOpen(ADesc *A, char *device, int mode, int freq, int nchannels, int encoding);
int SnackGetInputDevices(char **arr, int n);
char * SnackStrDup(const char *str);
/**********************************************************/

#define MAX_DEVICE_NAME_LENGTH 100
#define MAX_NUM_DEVICES 20

int main(void)
{

   int n, i = 0;
   char *arr[MAX_NUM_DEVICES];
   n = SnackGetInputDevices(arr, MAX_NUM_DEVICES);
   strcpy(defaultInDevice, arr[i]);
   SnackAudioOpen(&ado, defaultInDevice, RECORD, globalRate, 0, LIN16);
   return 0;
}

void SnackAudioOpen(ADesc *A, char *device, int mode, int freq, int nchannels, int encoding)
{
  int format;
  int nformat;
  int channels;
  int speed;
  int mask;

  snd_pcm_hw_params_t *hw_params;

  if (device == NULL) {
    device = defaultDeviceName;
  }
  if (strlen(device) == 0) {
    device = defaultDeviceName;
  }

  A->mode = mode;
  switch (mode) {
  case RECORD:
    if (snd_pcm_open(&A->handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0) {
      printf("Could not open device for read");
      return;
    }
    break;
  case PLAY:
    if (snd_pcm_open(&A->handle, device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
      printf("Could not open device for write");
      return;
    }
    break;
  }

  switch (encoding) {
  case LIN16:
    if (littleEndian) {
      format = SND_PCM_FORMAT_S16_LE;
    } else {
      format = SND_PCM_FORMAT_S16_BE;
    }
    A->bytesPerSample = sizeof(short);
    break;
  case LIN24:
    if (littleEndian) {
      format = SND_PCM_FORMAT_S32_LE;
    } else {
      format = SND_PCM_FORMAT_S32_BE;
    }
    A->bytesPerSample = sizeof(int);
    break;
  case ALAW:
    format = SND_PCM_FORMAT_A_LAW;
    A->bytesPerSample = sizeof(char);
    break;
  case MULAW:
      format = SND_PCM_FORMAT_MU_LAW;
      A->bytesPerSample = sizeof(char);
    break;
  case LIN8OFFSET:
    format = SND_PCM_FORMAT_U8;
    A->bytesPerSample = sizeof(char);
    break;
  case LIN8:
    format = SND_PCM_FORMAT_S8;
    A->bytesPerSample = sizeof(char);
    break;
  }

  snd_pcm_hw_params_malloc(&hw_params);
  snd_pcm_hw_params_any(A->handle, hw_params);
  snd_pcm_hw_params_set_access(A->handle, hw_params,
			       SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(A->handle, hw_params, format);
  snd_pcm_hw_params_set_rate_near(A->handle, hw_params, &freq, 0);
  snd_pcm_hw_params_set_channels(A->handle, hw_params, nchannels);

  if (snd_pcm_hw_params(A->handle, hw_params) < 0) {
    printf("Failed setting HW Params");
    return;
  }
  snd_pcm_hw_params_free(hw_params);
  snd_pcm_prepare(A->handle);
  if (A->mode == RECORD) {
    snd_pcm_start(A->handle);
  }


  /********QUICK DELAY FUNCTION TO KEEP MIC OPEN********************/
  int x, y, z, w, p = 0;
  for(x;x<10000009999999;x++){
     for(y;y<10000000999999;y++){
        for(z;z<100000009999999;z++){
           for(w;w<10000000999999;w++){
              for(p;p<10000000999999;p++);
           }
        }
     }
  }
  /***************************************************************/

  A->freq = freq;
  A->nWritten = 0;
  A->nPlayed = 0;

}

int SnackGetInputDevices(char **arr, int n)
{
  int i = -1, j = 0;
  char devicename[20];
  
  arr[j++] = (char *) SnackStrDup("default");
  while (snd_card_next(&i) == 0 && i > -1) {
    if (j < n) {
      snprintf(devicename, 20, "plughw:%d", i);
      arr[j++] = (char *) SnackStrDup(devicename);
    } else {
      break;
    }
  }
  return(j);
}

char * SnackStrDup(const char *str)
{
  char *new = malloc(strlen(str)+1);

  if (new) {
    strcpy(new, str);
  }

  return new;
}
