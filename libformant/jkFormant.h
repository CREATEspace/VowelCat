/*
 * This software has been licensed to the Centre of Speech Technology, KTH
 * by AT&T Corp. and Microsoft Corp. with the terms in the accompanying
 * file BSD.txt, which is a BSD style license.
 *
 *    "Copyright (c) 1987-1990  AT&T, Inc.
 *    "Copyright (c) 1986-1990  Entropic Speech, Inc.
 *    "Copyright (c) 1990-1991  Entropic Research Laboratory, Inc.
 *                   All rights reserved"
 */

 /* this is an older version of the waves tracks.h needed for this version
    of formant
 */

#define DEB_PAUSE	8	/* debug levels */
#define DEB_LPC_PARS	4
#define DEB_PARAMS	2
#define DEB_ENTRY	1
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define PI 3.1415927
#define MAXFORMANTS 7

typedef struct Sound {
    int    samprate;
    int    encoding;
    int    sampsize;
    int    nchannels;
    int    length;
    int    maxlength;
    float  maxsamp;
    float  minsamp;
    float  abmax;
    float  **blocks;
    int    maxblks;
    int    nblks;
    int    exact;
    int    precision;
    int    writeStatus;
    int    readStatus;
    short  *tmpbuf;
    int    swap;
    int    headSize;
    int    skipBytes;
    int    buffersize;
    char           *fcname;
    struct jkCallback *firstCB;
    char *fileType;
    int blockingPlay;
    int debug;
    int destroy;
    int guessEncoding;
    int inByteOrder;
    int firstNRead;
    int guessRate;
    int forceFormat;
    int itemRefCnt;
    int validStart;
    char *devStr;
    char *filterName;
    char *extHead;
    char *extHead2;
    int extHeadType;
    int extHead2Type;
    int loadOffset;
    unsigned int userFlag; /* User flags, for new file formats, etc */
    char *userData;        /* User data pointer */

} Sound;

typedef struct {
    size_t len;
    char *bytes;
} Tcl_Obj;

enum {
    LIN16,
    SNACK_FLOAT,
};

enum {
    SNACK_SINGLE_PREC,
    SNACK_DOUBLE_PREC,
};

/* structure definitions for the pitch tracker */
#define CROSS  struct cross_rec
struct cross_rec { /* for storing the crosscorrelation information */
    double	rms;	/* rms energy in the reference window */
    double	k1;	/* 1st-order autoregressive flattening constant. */
    double	maxval;	/* max in the crosscorr. fun. q15 */
    short	maxloc; /* lag # at which max occured	*/
    short	nlags;	/* the number of correlation lags computed */
    short	firstlag; /* the first non-zero lag computed */
    short	*correl; /* the normalized corsscor. fun. q15 */
};

#define DPREC struct dp_rec
struct dp_rec { /* for storing the DP information */
    short	ncands;	/* # of candidate pitch intervals in the frame */
    short	*locs; /* locations of the candidates */
    short	*pvals; /* peak values of the candidates */
    double	*mpvals; /* modified peak values of the candidates */
    short	*prept; /* pointers to best previous cands. */
    double	*dpvals; /* cumulative error for each candidate */
};
/* end of structure definitions for the pitch tracker */

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* Structure definitions for the formant tracker.. */

typedef struct form_latt { /* structure of a DP lattice node for formant tracking */
    short ncand; /* # of candidate mappings for this frame */
    short **cand;      /* pole-to-formant map-candidate array */
    short *prept;	 /* backpointer array for each frame */
    double *cumerr; 	 /* cum. errors associated with each cand. */
} FORM;

typedef struct pole_array {   /* structure to hold raw LPC analysis data */
    double rms;    /* rms for current LPC analysis frame */
    double rms2;    /* rms for current F0 analysis frame */
    double f0;     /* fundamental frequency estimate for this frame */
    double pv;		/* probability that frame is voiced */
    double change; /* spec. distance between current and prev. frames */
    short npoles; /* # of complex poles from roots of LPC polynomial */
    double *freq;  /* array of complex pole frequencies (Hz) */
    double *band;  /* array of complex pole bandwidths (Hz) */
} POLE;
/* End of structure definitions for the formant tracker. */

Sound *Snack_NewSound(int rate, int encoding, int nchannels);
void Snack_DeleteSound(Sound *s);
void LoadSound(Sound *s, Tcl_Obj *obj, int startpos, int endpos);
