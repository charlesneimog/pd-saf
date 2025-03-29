#include <string.h>

#include <m_pd.h>
#include <g_canvas.h>

#include <ambi_enc.h>

static t_class *encoder_tilde_class;

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
} t_encoder_tilde;

// ─────────────────────────────────────
static void encoder_tilde_malloc(t_encoder_tilde *x) {
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
static void encoder_tilde_set(t_encoder_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;
    if (strcmp(method, "postscaling") == 0) {
        int postScaling = atom_getint(argv + 1);
        ambi_enc_setEnablePostScaling(x->hAmbi, postScaling);
    } else if (strcmp(method, "solo") == 0) {
        int srcIdx = atom_getint(argv + 1); // Source index
        int solo = atom_getint(argv + 2);   // Solo status
        if (solo) {
            ambi_enc_setSourceSolo(x->hAmbi, srcIdx);
        } else {
            ambi_enc_setUnSolo(x->hAmbi);
        }
    } else if (strcmp(method, "norm_type") == 0) {
        int newType = atom_getint(argv + 1);
        if (newType < 1 || newType > 3) {
            logpost(x, 1, "[saf.encoder~] norm_type must be 1-3");
            logpost(x, 2, "               N3D  = 1");
            logpost(x, 2, "               SN3D = 2");
            logpost(x, 2, "               FUMA = 3");
            return;
        }
        ambi_enc_setNormType(x->hAmbi, newType);
    } else if (strcmp(method, "source_gain") == 0) {
        int srcIdx = atom_getint(argv + 1);      // Source index
        float newGain = atom_getfloat(argv + 2); // Gain factor
        ambi_enc_setSourceGain(x->hAmbi, srcIdx, newGain);
    } else if (strcmp(method, "num_sources") == 0) {
        int state = canvas_suspend_dsp();
        int sources = atom_getint(argv + 1);
        ambi_enc_setNumSources(x->hAmbi, sources);
        x->nIn = sources;
        canvas_update_dsp();
        canvas_resume_dsp(state);
    } else{
        pd_error(x, "[saf.encoder~] Unknown set method: %s", method);
    }
}

// ─────────────────────────────────────
void encoder_tilde_set_source(t_encoder_tilde *x, t_floatarg idx, t_floatarg azi, t_floatarg elev) {
    if (idx < 0 || idx - 1 >= x->nIn) {
        pd_error(x, "[saf.encoder~] Source index %d out of range (0-%d)", (int)idx, x->nIn - 1);
        return;
    }
    ambi_enc_setSourceAzi_deg(x->hAmbi, (int)idx - 1, azi);
    ambi_enc_setSourceElev_deg(x->hAmbi, (int)idx - 1, elev);
    ambi_enc_refreshParams(x->hAmbi);
}

// ─────────────────────────────────────
t_int *encoder_tilde_performmultichannel(t_int *w) {
    t_encoder_tilde *x = (t_encoder_tilde *)(w[1]);
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
            ambi_enc_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            ambi_enc_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
t_int *encoder_tilde_perform(t_int *w) {
    t_encoder_tilde *x = (t_encoder_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_enc_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            ambi_enc_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
void encoder_tilde_dsp(t_encoder_tilde *x, t_signal **sp) {
    // This is a mess! Help is you see a better way.

    // ambi_enc_getFrameSize has fixed frameSize, for encoder is 64 for
    // decoder is 128. In the perform method sometimes I need to accumulate samples sometimes I
    // need to process 2 or more times to avoid change how ambi_enc_ works. I think that in this
    // way is more safe, once that these functions are tested in the main repo. But maybe worse
    // to implement the own set of functions.

    // Set frame sizes and reset indices
    x->nAmbiFrameSize = ambi_enc_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;
    int sum = x->nIn + x->nOut;
    int sigvecsize = sum + 2;

    // Initialize the ambisonic encoder
    if (!x->hAmbiInit) {
        ambi_enc_init(x->hAmbi, sys_getsr());
        ambi_enc_setOutputOrder(x->hAmbi, (SH_ORDERS)x->nOrder);
        ambi_enc_setNumSources(x->hAmbi, x->nIn);
        if (ambi_enc_getNSHrequired(x->hAmbi) < x->nOut) {
            pd_error(x, "[saf.encoder~] Number of output signals is too low for the %d order.",
                     x->nOrder);
            return;
        }
        x->hAmbiInit = 1;
    }

    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        ambi_enc_setNumSources(x->hAmbi, x->nIn);
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        encoder_tilde_malloc(x);
    }

    // add perform method
    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        ambi_enc_setNumSources(x->hAmbi, x->nIn);
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(encoder_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
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
        dsp_addv(encoder_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *encoder_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_encoder_tilde *x = (t_encoder_tilde *)pd_new(encoder_tilde_class);
    int order = (argc >= 1) ? atom_getint(argv) : 1;
    int num_sources = (argc >= 2) ? atom_getint(argv + 1) : 1;
    x->multichannel = (argc >= 3) ? strcmp(atom_getsymbol(argv + 2)->s_name, "-m") == 0 : 0;
    if (argc < 2) {
        pd_error(x, "[saf.encoder~] Wrong number of arguments, use [saf.encoder~ <speakers_count> "
                    "<sources>");
        return NULL;
    }

    order = order < 1 ? 1 : order;
    num_sources = num_sources < 1 ? 1 : num_sources;
    x->hAmbiInit = 0;

    ambi_enc_create(&x->hAmbi);
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
void encoder_tilde_free(t_encoder_tilde *x) {
    ambi_enc_destroy(&x->hAmbi);
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
void setup_saf0x2eencoder_tilde(void) {
    encoder_tilde_class = class_new(gensym("saf.encoder~"), (t_newmethod)encoder_tilde_new,
                                    (t_method)encoder_tilde_free, sizeof(t_encoder_tilde),
                                    CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    logpost(NULL, 3, "[saf] is a pd version of Spatial Audio Framework by Leo McCormack");
    logpost(NULL, 3, "[saf] pd-saf by Charles K. Neimog");

    CLASS_MAINSIGNALIN(encoder_tilde_class, t_encoder_tilde, sample);
    class_addmethod(encoder_tilde_class, (t_method)encoder_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(encoder_tilde_class, (t_method)encoder_tilde_set_source, gensym("source"),
                    A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(encoder_tilde_class, (t_method)encoder_tilde_set, gensym("set"), A_GIMME, 0);
}
