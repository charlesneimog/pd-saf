#include <string.h>

#include <m_pd.h>
#include <g_canvas.h>

#include <multiconv.h>

static t_class *multiconv_tilde_class;

// ─────────────────────────────────────
typedef struct _multiconv_tilde {
    t_object obj;
    t_sample sample;

    void *hMCnv;
    unsigned hMCnvInit;

    t_sample **aIns;
    t_sample **aOuts;
    t_sample **aInsTmp;
    t_sample **aOutsTmp;

    int nFrameSize;
    int nPdFrameSize;
    int nInAccIndex;
    int nOutAccIndex;

    int nIn;
    int nOut;
    int nPreviousIn;
    int nPreviousOut;

    int multichannel;
} t_multiconv_tilde;

// ─────────────────────────────────────
static void multiconv_tilde_malloc(t_multiconv_tilde *x) {
    if (x->aIns) {
        for (int i = 0; i < x->nPreviousIn; i++) {
            freebytes(x->aIns[i], x->nFrameSize * sizeof(t_sample));
            freebytes(x->aInsTmp[i], x->nFrameSize * sizeof(t_sample));
        }
        freebytes(x->aIns, x->nIn * sizeof(t_sample *));
    }

    if (x->aOuts) {
        for (int i = 0; i < x->nPreviousOut; i++) {
            freebytes(x->aOuts[i], x->nFrameSize * sizeof(t_sample));
            freebytes(x->aOutsTmp[i], x->nFrameSize * sizeof(t_sample));
        }
        freebytes(x->aOuts, x->nOut * sizeof(t_sample *));
    }

    x->aIns = (t_sample **)getbytes(x->nIn * sizeof(t_sample *));
    x->aInsTmp = (t_sample **)getbytes(x->nIn * sizeof(t_sample *));
    x->aOuts = (t_sample **)getbytes(x->nOut * sizeof(t_sample *));
    x->aOutsTmp = (t_sample **)getbytes(x->nOut * sizeof(t_sample *));

    for (int i = 0; i < x->nIn; i++) {
        x->aIns[i] = (t_sample *)getbytes(x->nFrameSize * sizeof(t_sample));
        x->aInsTmp[i] = (t_sample *)getbytes(x->nFrameSize * sizeof(t_sample));
    }

    for (int i = 0; i < x->nOut; i++) {
        x->aOuts[i] = (t_sample *)getbytes(x->nFrameSize * sizeof(t_sample));
        x->aOutsTmp[i] = (t_sample *)getbytes(x->nFrameSize * sizeof(t_sample));
    }

    x->nPreviousIn = x->nIn;
    x->nPreviousOut = x->nOut;
}

// ─────────────────────────────────────
void multiconv_tilde_set_ir(t_multiconv_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    if (argc < 1) {
        pd_error(x, "[saf.multiconv~] usage: ir <array_name>");
        return;
    }

    t_symbol *array_name = atom_getsymbol(argv);
    t_garray *pdarray = (t_garray *)pd_findbyclass(array_name, garray_class);
    if (!pdarray) {
        pd_error(x, "[saf.multiconv~] array not found");
        return;
    }

    t_word *vec = NULL;
    int vecsize = 0;

    if (!garray_getfloatwords(pdarray, &vecsize, &vec)) {
        pd_error(x, "[saf.multiconv~] bad array format");
        return;
    }

    if (vecsize <= 0) {
        pd_error(x, "[saf.multiconv~] empty impulse response");
        return;
    }

    /* allocate IR buffer (mono IR replicated to all channels) */
    float **H = (float **)getbytes(sizeof(float *) * x->nOut);
    float *ir = (float *)getbytes(sizeof(float) * vecsize);

    for (int i = 0; i < vecsize; i++) {
        ir[i] = (float)vec[i].w_float;
    }

    for (int ch = 0; ch < x->nOut; ch++) {
        H[ch] = ir;
    }

    multiconv_setFilters(x->hMCnv, (const float *const *)H, x->nOut, vecsize, sys_getsr());

    multiconv_refreshParams(x->hMCnv);

    /* cleanup wrapper only (SAF copies internally) */
    freebytes(H, sizeof(float *) * x->nOut);
    freebytes(ir, sizeof(float) * vecsize);

    logpost(x, PD_NORMAL, "[saf.multiconv~] IR updated with sample size %d", vecsize);
}

// ─────────────────────────────────────
t_int *multiconv_tilde_performmultichannel(t_int *w) {
    t_multiconv_tilde *x = (t_multiconv_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    if (n < x->nFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, ins + (n * ch), n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        // Process only if a full frame is ready
        if (x->nInAccIndex == x->nFrameSize) {
            multiconv_process(x->hMCnv, (const float *const *)x->aIns, (float *const *)x->aOuts,
                              x->nIn, x->nOut, x->nFrameSize);
            x->nInAccIndex = 0;
            x->nOutAccIndex = 0; // Reset for the next frame
        }

        if (x->nOutAccIndex + n <= x->nFrameSize) {
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
        int chunks = n / x->nFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            // Copia os dados de entrada para cada canal
            for (int ch = 0; ch < x->nIn; ch++) {
                memcpy(x->aInsTmp[ch], (t_sample *)w[3] + ch * n + chunkIndex * x->nFrameSize,
                       x->nFrameSize * sizeof(t_sample));
            }
            // Processa o bloco atual
            multiconv_process(x->hMCnv, (const float *const *)x->aInsTmp,
                              (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nFrameSize);

            t_sample *out = (t_sample *)(w[4]);
            // Copia o resultado para os canais de saída com o offset correto
            for (int ch = 0; ch < x->nOut; ch++) {
                memcpy(out + ch * n + chunkIndex * x->nFrameSize, x->aOutsTmp[ch],
                       x->nFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 5);
}

// ─────────────────────────────────────
t_int *multiconv_tilde_perform(t_int *w) {
    t_multiconv_tilde *x = (t_multiconv_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;
        if (x->nInAccIndex == x->nFrameSize) {
            multiconv_process(x->hMCnv, (const float *const *)x->aIns, (float *const *)x->aOuts,
                              x->nIn, x->nOut, x->nFrameSize);
            x->nInAccIndex = 0;
            x->nOutAccIndex = 0;
        }
        for (int ch = 0; ch < x->nOut; ch++) {
            t_sample *out = (t_sample *)(w[3 + x->nIn + ch]);
            memcpy(out, x->aOuts[ch] + x->nOutAccIndex, n * sizeof(t_sample));
        }
        x->nOutAccIndex += n;
    } else {
        int chunks = n / x->nFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            for (int ch = 0; ch < x->nIn; ch++) {
                memcpy(x->aInsTmp[ch], (t_sample *)w[3 + ch] + (chunkIndex * x->nFrameSize),
                       x->nFrameSize * sizeof(t_sample));
            }
            multiconv_process(x->hMCnv, (const float *const *)x->aInsTmp,
                              (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nFrameSize);
            for (int ch = 0; ch < x->nOut; ch++) {
                t_sample *out = (t_sample *)(w[3 + x->nIn + ch]);
                memcpy(out + (chunkIndex * x->nFrameSize), x->aOutsTmp[ch],
                       x->nFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 3 + x->nIn + x->nOut);
}

// ─────────────────────────────────────
void multiconv_tilde_dsp(t_multiconv_tilde *x, t_signal **sp) {
    // ambi_enc_getFrameSize has fixed frameSize, for encoder is 64 for
    // decoder is 128. In the perform method sometimes I need to accumulate
    // samples sometimes I need to process 2 or more times to avoid change how
    // ambi_enc_ works. I think that in this way is more safe, once that these
    // functions are tested in the main repo. But maybe worse to implement the own
    // set of functions.

    x->nFrameSize = 8192;
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;

    x->nIn = x->multichannel ? sp[0]->s_nchans : x->nIn;

    int sum = x->nIn + x->nOut;
    int sigvecsize = sum + 2;

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        multiconv_tilde_malloc(x);
        x->nPreviousIn = x->nIn;
    }

    // add perform method
    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(multiconv_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
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
        dsp_addv(multiconv_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *multiconv_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_multiconv_tilde *x = (t_multiconv_tilde *)pd_new(multiconv_tilde_class);

    x->multichannel = 0;

    int nIn = 1;
    int nOut = 1;

    if (argc >= 2) {
        nIn = atom_getint(argv);
        nOut = atom_getint(argv + 1);
    }

    x->nIn = nIn;
    x->nOut = nOut;

    multiconv_create(&x->hMCnv);
    multiconv_init(x->hMCnv, sys_getsr(), 8192);

    for (int i = 1; i < x->nIn; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->nOut; i++) {
        outlet_new(&x->obj, &s_signal);
    }
    x->aIns = NULL;
    x->aOuts = NULL;
    x->aInsTmp = NULL;
    x->aOutsTmp = NULL;

    x->nPreviousIn = 0;
    x->nPreviousOut = 0;

    return x;
}

// ─────────────────────────────────────
void multiconv_tilde_free(t_multiconv_tilde *x) {
    multiconv_destroy(&x->hMCnv);

    for (int i = 0; i < x->nIn; i++) {
        freebytes(x->aIns[i], x->nFrameSize * sizeof(t_sample));
        freebytes(x->aInsTmp[i], x->nFrameSize * sizeof(t_sample));
    }

    for (int i = 0; i < x->nOut; i++) {
        freebytes(x->aOuts[i], x->nFrameSize * sizeof(t_sample));
        freebytes(x->aOutsTmp[i], x->nFrameSize * sizeof(t_sample));
    }
}

// ─────────────────────────────────────
void setup_saf0x2emulticonvolve_tilde(void) {
    multiconv_tilde_class =
        class_new(gensym("saf.multiconvolve~"), (t_newmethod)multiconv_tilde_new,
                  (t_method)multiconv_tilde_free, sizeof(t_multiconv_tilde),
                  CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(multiconv_tilde_class, t_multiconv_tilde, sample);
    class_addmethod(multiconv_tilde_class, (t_method)multiconv_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(multiconv_tilde_class, (t_method)multiconv_tilde_set_ir, gensym("ir"), A_GIMME,
                    0);
}
