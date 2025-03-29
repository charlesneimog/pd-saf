#include <string.h>
#include <math.h>
#include <pthread.h>

#include <m_pd.h>
#include <g_canvas.h>

#include <pitch_shifter.h>

static t_class *pitchshifter_tilde_class;

// ─────────────────────────────────────
typedef struct _pitchshifter_tilde {
    t_object obj;
    t_sample sample;

    void *hAmbi;
    unsigned hAmbiInit;

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
} t_pitchshifter_tilde;

// ─────────────────────────────────────
static void pitchshifter_tilde_malloc(t_pitchshifter_tilde *x) {
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
static void pitchshifter_tilde_set(t_pitchshifter_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;

    if (strcmp(method, "cents") == 0) {
        float cents = atom_getfloat(argv + 1);
        float factor = pow(2, cents / 1200.0);
        post("factor is %f", factor);
        pitch_shifter_setPitchShiftFactor(x->hAmbi, factor);
    } else if (strcmp(method, "factor") == 0) {
        float factor = atom_getfloat(argv + 1);
        pitch_shifter_setPitchShiftFactor(x->hAmbi, factor);
    } else if (strcmp(method, "osamp") == 0) {
        float osamp = atom_getfloat(argv + 1);
        pitch_shifter_setOSampOption(x->hAmbi, osamp);
    } else if (strcmp(method, "fftsize") == 0) {
        // TODO: NEED TO REALLOC MEMORY THE FFT SIZE
    }
}

// ─────────────────────────────────────
t_int *pitchshifter_tilde_performmultichannel(t_int *w) {
    t_pitchshifter_tilde *x = (t_pitchshifter_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, ins + (n * ch), n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        // Process only if a full frame is ready
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            pitch_shifter_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
                                  x->nIn, x->nOut, x->nAmbiFrameSize);
            x->nInAccIndex = 0;
            x->nOutAccIndex = 0; // Reset for the next frame
        }

        if (x->nOutAccIndex + n <= x->nAmbiFrameSize) {
            // Copy valid processed data
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
            pitch_shifter_process(x->hAmbi, (const float *const *)x->aInsTmp,
                                  (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nAmbiFrameSize);

            t_sample *out = (t_sample *)(w[4]);
            // Copia o resultado para os canais de saída com o offset correto
            for (int ch = 0; ch < x->nOut; ch++) {
                memcpy(out + ch * n + chunkIndex * x->nAmbiFrameSize, x->aOutsTmp[ch],
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 5);
}

// ─────────────────────────────────────
t_int *pitchshifter_tilde_perform(t_int *w) {
    t_pitchshifter_tilde *x = (t_pitchshifter_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            pitch_shifter_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            pitch_shifter_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
void pitchshifter_tilde_dsp(t_pitchshifter_tilde *x, t_signal **sp) {
    // This is a mess! Help is you see a better way.

    // pitch_shifter_getFrameSize has fixed frameSize, for pitchshifter is 64 for
    // decoder is 128. In the perform method sometimes I need to accumulate samples sometimes I
    // need to process 2 or more times to avoid change how pitch_shifter_ works. I think that in
    // this way is more safe, once that these functions are tested in the main repo. But maybe worse
    // to implement the own set of functions.

    // Set frame sizes and reset indices
    x->nAmbiFrameSize = pitch_shifter_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;
    int sum = x->nIn + x->nOut;
    int sigvecsize = sum + 2;

    // Initialize the ambisonic pitchshifter
    if (!x->hAmbiInit) {
        pitch_shifter_init(x->hAmbi, sys_getsr());
        x->hAmbiInit = 1;
    }

    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        pitch_shifter_setNumChannels(x->hAmbi, x->nIn);
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        pitchshifter_tilde_malloc(x);
    }

    // add perform method
    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(pitchshifter_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec,
                sp[1]->s_vec);
    } else {
        for (int i = x->nIn; i < sum; i++) {
            signal_setmultiout(&sp[i], 1);
        }
        t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));
        sigvec[0] = (t_int)x;
        sigvec[1] = (t_int)sp[0]->s_n;
        for (int i = 0; i < sum; i++) {
            sigvec[2 + i] = (t_int)sp[i]->s_vec;
        }
        dsp_addv(pitchshifter_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *pitchshifter_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_pitchshifter_tilde *x = (t_pitchshifter_tilde *)pd_new(pitchshifter_tilde_class);
    int order = (argc >= 1) ? atom_getint(argv) : 1;
    int num_sources = (argc >= 2) ? atom_getint(argv + 1) : 1;
    x->multichannel = (argc >= 3) ? strcmp(atom_getsymbol(argv + 2)->s_name, "-m") == 0 : 0;
    if (argc < 2) {
        pd_error(x, "[saf.pitchshifter~] Wrong number of arguments, use [saf.pitchshifter~ "
                    "<speakers_count> "
                    "<sources>");
        return NULL;
    }

    order = order < 1 ? 1 : order;
    num_sources = num_sources < 1 ? 1 : num_sources;
    x->hAmbiInit = 0;

    pitch_shifter_create(&x->hAmbi);
    pitch_shifter_setNumChannels(x->hAmbi, num_sources);
    x->nOrder = order;
    x->nIn = num_sources;
    x->nOut = (order + 1) * (order + 1);
    x->nInAccIndex = 0;

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

    return (void *)x;
}

// ─────────────────────────────────────
void pitchshifter_tilde_free(t_pitchshifter_tilde *x) {
    pitch_shifter_destroy(&x->hAmbi);
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
void setup_saf0x2epitchshifter_tilde(void) {
    pitchshifter_tilde_class =
        class_new(gensym("saf.pitchshifter~"), (t_newmethod)pitchshifter_tilde_new,
                  (t_method)pitchshifter_tilde_free, sizeof(t_pitchshifter_tilde),
                  CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(pitchshifter_tilde_class, t_pitchshifter_tilde, sample);
    class_addmethod(pitchshifter_tilde_class, (t_method)pitchshifter_tilde_dsp, gensym("dsp"),
                    A_CANT, 0);
    class_addmethod(pitchshifter_tilde_class, (t_method)pitchshifter_tilde_set, gensym("set"),
                    A_GIMME, 0);
}
