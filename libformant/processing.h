#ifndef PROCESSING_H
#define PROCESSING_H

int formant(int lpc_order, double s_freq, double *lpca, int *n_form,
            double *freq, double *band, int init);

int w_covar(short *xx, int *m, int n, int istrt, double *y, double *alpha,
            double *r0, double preemp, int w_type);

int lpc(int lpc_ord, double lpc_stabl, int wsize, short *data, double *lpca,
        double *ar, double *lpck, double *normerr, double *rms, double preemp,
        int type);

int dlpcwtd(double *s, int *ls, double *p, int *np, double *c, double *phi,
            double *shi, double *xl, double *w);

#endif
