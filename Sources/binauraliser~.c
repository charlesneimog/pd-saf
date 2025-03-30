#include <string.h>
#include <math.h>
#include <pthread.h>

#include <m_pd.h>
#include <g_canvas.h>
#include <s_stuff.h>

#include <binauraliser.h>
#include "utilities.h"

static t_class *binauraliser_tilde_class;

// ─────────────────────────────────────
typedef struct _binauraliser_tilde {
    t_object obj;
    t_canvas *glist;
    t_sample sample;

    void *hAmbi;
    int hAmbiInit;

    t_sample **aIns;
    t_sample **aOuts;
    t_sample **aInsTmp;
    t_sample **aOutsTmp;

    int nAmbiFrameSize;
    int nPdFrameSize;
    int nInAccIndex;
    int nOutAccIndex;

    int nOrder;
    int nIn;
    int nOut;
    int nPreviousIn;
    int nPreviousOut;

    int multichannel;
} t_binauraliser_tilde;

// ─────────────────────────────────────
static void binauraliser_tilde_malloc(t_binauraliser_tilde *x) {
    if (x->aIns) {
        for (int i = 0; i < x->nIn; i++) {
            if (x->aIns[i]) {
                freebytes(x->aIns[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
            if (x->aInsTmp[i]) {
                freebytes(x->aInsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aIns, x->nIn * sizeof(t_sample *));
    }
    if (x->aOuts) {
        for (int i = 0; i < x->nOut; i++) {
            if (x->aOuts[i]) {
                freebytes(x->aOuts[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
            if (x->aOutsTmp[i]) {
                freebytes(x->aOutsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aOuts, x->nOut * sizeof(t_sample *));
    }

    // memory allocation
    x->aIns = (t_sample **)getbytes(x->nIn * sizeof(t_sample *));
    x->aInsTmp = (t_sample **)getbytes(x->nIn * sizeof(t_sample *));
    x->aOuts = (t_sample **)getbytes(x->nOut * sizeof(t_sample *));
    x->aOutsTmp = (t_sample **)getbytes(x->nOut * sizeof(t_sample *));

    for (int i = 0; i < x->nIn; i++) {
        x->aIns[i] = (t_sample *)getbytes(x->nAmbiFrameSize * sizeof(t_sample));
        x->aInsTmp[i] = (t_sample *)getbytes(x->nAmbiFrameSize * sizeof(t_sample));
    }
    for (int i = 0; i < x->nOut; i++) {
        x->aOuts[i] = (t_sample *)getbytes(x->nAmbiFrameSize * sizeof(t_sample));
        x->aOutsTmp[i] = (t_sample *)getbytes(x->nAmbiFrameSize * sizeof(t_sample));
    }
    x->nPreviousIn = x->nIn;
    x->nPreviousOut = x->nOut;
}

// ─────────────────────────────────────
void *binauraliser_tilde_initcodec(void *x_void) {
    t_binauraliser_tilde *x = (t_binauraliser_tilde *)x_void;
    binauraliser_initCodec(x->hAmbi);
    logpost(x, 2, "[saf.binauraliser~] decoder codec initialized!");
    return NULL;
}

// ╭─────────────────────────────────────╮
// │               Methods               │
// ╰─────────────────────────────────────╯
static void binauraliser_tilde_set(t_binauraliser_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;
    if (strcmp(method, "sofafile") == 0) {
        char path[MAXPDSTRING];
        char *bufptr;
        t_symbol *sofa_path = atom_getsymbol(argv + 1);
        int fd = canvas_open(x->glist, sofa_path->s_name, "", path, &bufptr, MAXPDSTRING, 1);
        if (fd > 1) {
            char completpath[MAXPDSTRING];
            pd_snprintf(completpath, MAXPDSTRING, "%s/%s", path, sofa_path->s_name);
            logpost(x, 2, "[saf.binauraliser~] Opening %s", completpath);
            binauraliser_setSofaFilePath(x->hAmbi, completpath);
        } else {
            pd_error(x->glist, "[saf.binauraliser~] Could not open sofa file!");
        }
    } else if (strcmp(method, "defaultHRIR") == 0) {
        t_float defaultHRIR = atom_getfloat(argv + 1);
        binauraliser_setUseDefaultHRIRsflag(x->hAmbi, defaultHRIR);
    } else {
        pd_error(x, "[saf.binauraliser~] Unknown set method: %s", method);
    }
}

// ╭─────────────────────────────────────╮
// │     Initialization and Perform      │
// ╰─────────────────────────────────────╯
t_int *binauraliser_tilde_performmultichannel(t_int *w) {
    t_binauraliser_tilde *x = (t_binauraliser_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, ins + (n * ch), n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        if (x->nInAccIndex == x->nAmbiFrameSize) {
            binauraliser_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
                                 x->nIn, x->nOut, x->nAmbiFrameSize);
            x->nInAccIndex = 0;
            x->nOutAccIndex = 0;
        }

        if (x->nOutAccIndex + n <= x->nAmbiFrameSize) {
            for (int ch = 0; ch < x->nOut; ch++) {
                memcpy(outs + (n * ch), x->aOuts[ch] + x->nOutAccIndex, n * sizeof(t_sample));
            }
            x->nOutAccIndex += n;
        } else {
            for (int ch = 0; ch < x->nOut; ch++) {
                memset(outs + (n * ch), 0, n * sizeof(t_sample));
            }
        }
    } else {
        int chunks = n / x->nAmbiFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            // Copia os dados de entrada para cada canal
            for (int ch = 0; ch < x->nIn; ch++) {
                memcpy(x->aInsTmp[ch], (t_sample *)w[3] + ch * n + chunkIndex * x->nAmbiFrameSize,
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
            // Processa o bloco atual
            binauraliser_process(x->hAmbi, (const float *const *)x->aInsTmp,
                                 (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nAmbiFrameSize);

            t_sample *out = (t_sample *)(w[4]);
            for (int ch = 0; ch < x->nOut; ch++) {
                memcpy(out + ch * n + chunkIndex * x->nAmbiFrameSize, x->aOutsTmp[ch],
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 5);
}

// ─────────────────────────────────────
t_int *binauraliser_tilde_perform(t_int *w) {
    t_binauraliser_tilde *x = (t_binauraliser_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            binauraliser_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
                                 x->nIn, x->nOut, x->nAmbiFrameSize);
            x->nInAccIndex = 0;
            x->nOutAccIndex = 0;
        }
        for (int ch = 0; ch < x->nOut; ch++) {
            t_sample *out = (t_sample *)(w[3 + x->nIn + ch]);
            memcpy(out, x->aOuts[ch] + x->nOutAccIndex, n * sizeof(t_sample));
        }
        x->nOutAccIndex += n;
    } else {
        int chunks = n / x->nAmbiFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            for (int ch = 0; ch < x->nIn; ch++) {
                memcpy(x->aInsTmp[ch], (t_sample *)w[3 + ch] + (chunkIndex * x->nAmbiFrameSize),
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
            binauraliser_process(x->hAmbi, (const float *const *)x->aInsTmp,
                                 (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nAmbiFrameSize);
            for (int ch = 0; ch < x->nOut; ch++) {
                t_sample *out = (t_sample *)(w[3 + x->nIn + ch]);
                memcpy(out + (chunkIndex * x->nAmbiFrameSize), x->aOutsTmp[ch],
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 3 + x->nIn + x->nOut);
}

// ─────────────────────────────────────
void binauraliser_tilde_dsp(t_binauraliser_tilde *x, t_signal **sp) {
    // Set frame sizes and reset indices
    x->nAmbiFrameSize = binauraliser_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;
    x->nIn = x->multichannel ? sp[0]->s_nchans : x->nIn;

    int nOrder = get_ambisonic_order(x->nOut);
    if (nOrder != x->nOrder || !x->hAmbiInit) {
        binauraliser_setUseDefaultHRIRsflag(x->hAmbi, 1);
        binauraliser_init(x->hAmbi, sys_getsr());
        logpost(x, 2, "[saf.decoder~] initializing decoder codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, binauraliser_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
        x->hAmbiInit = 1;
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        binauraliser_tilde_malloc(x);
    }

    // Initialize memory allocation for inputs and outputs
    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(binauraliser_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec,
                sp[1]->s_vec);
    } else {
        int sum = x->nIn + x->nOut;
        int sigvecsize = sum + 2;
        for (int i = x->nIn; i < sum; i++) {
            signal_setmultiout(&sp[i], 1);
        }
        t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));
        sigvec[0] = (t_int)x;
        sigvec[1] = (t_int)sp[0]->s_n;
        for (int i = 0; i < sum; i++) {
            sigvec[2 + i] = (t_int)sp[i]->s_vec;
        }
        dsp_addv(binauraliser_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *binauraliser_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_binauraliser_tilde *x = (t_binauraliser_tilde *)pd_new(binauraliser_tilde_class);
    x->glist = canvas_getcurrent(); // TODO: add HRIR reader
    int order = (argc >= 1) ? atom_getint(argv) : 1;
    int num_loudspeakers = (argc >= 2) ? atom_getint(argv + 1) : 2;
    x->multichannel = (argc >= 3) ? strcmp(atom_getsymbol(argv + 2)->s_name, "-m") == 0 : 0;

    if (argc >= 1) {
        order = atom_getint(argv);
    }
    if (argc >= 2) {
        num_loudspeakers = atom_getint(argv + 1);
    }

    order = order < 0 ? 0 : order;
    num_loudspeakers = num_loudspeakers < 1 ? 1 : num_loudspeakers;

    x->nOrder = (int)floor(sqrt(num_loudspeakers) - 1);
    x->nIn = (order + 1) * (order + 1);
    x->nOut = num_loudspeakers;
    binauraliser_create(&x->hAmbi);

    if (x->multichannel) {
        outlet_new(&x->obj, &s_signal);
    } else {
        for (int i = 1; i < x->nIn; i++) {
            inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
        }

        for (int i = 0; i < x->nOut; i++) {
            outlet_new(&x->obj, &s_signal);
        }
    }
    x->aIns = NULL;
    x->aOuts = NULL;

    return (void *)x;
}

// ─────────────────────────────────────
void binauraliser_tilde_free(t_binauraliser_tilde *x) {
    binauraliser_destroy(&x->hAmbi);
    for (int i = 0; i < x->nIn; i++) {
        if (x->aIns) {
            freebytes(x->aIns[i], x->nAmbiFrameSize * sizeof(t_sample));
        }
        if (x->aInsTmp) {
            freebytes(x->aInsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
        }
    }
    for (int i = 0; i < x->nOut; i++) {
        if (x->aOuts) {
            freebytes(x->aOuts[i], x->nAmbiFrameSize * sizeof(t_sample));
        }
        if (x->aOutsTmp) {
            freebytes(x->aOutsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
        }
    }

    if (x->aIns) {
        freebytes(x->aIns, x->nIn * sizeof(t_sample *));
    }
    if (x->aInsTmp) {
        freebytes(x->aInsTmp, x->nIn * sizeof(t_sample *));
    }
    if (x->aOuts) {
        freebytes(x->aOuts, x->nOut * sizeof(t_sample *));
    }
    if (x->aOutsTmp) {
        freebytes(x->aOutsTmp, x->nOut * sizeof(t_sample *));
    }
}

// ─────────────────────────────────────
void setup_saf0x2ebinauraliser_tilde(void) {
    binauraliser_tilde_class =
        class_new(gensym("saf.binauraliser~"), (t_newmethod)binauraliser_tilde_new,
                  (t_method)binauraliser_tilde_free, sizeof(t_binauraliser_tilde),
                  CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(binauraliser_tilde_class, t_binauraliser_tilde, sample);
    class_addmethod(binauraliser_tilde_class, (t_method)binauraliser_tilde_dsp, gensym("dsp"),
                    A_CANT, 0);
    class_addmethod(binauraliser_tilde_class, (t_method)binauraliser_tilde_set, gensym("set"),
                    A_GIMME, 0);
}
