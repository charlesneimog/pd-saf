#include <string.h>

#include <m_pd.h>
#include <g_canvas.h>

#include <ambi_enc.h>

static t_class *encoder_tilde_class;

// ─────────────────────────────────────
typedef struct _saf {
    t_object obj;
    t_sample sample;

    void *hAmbi;
    unsigned ambi_init;

    t_sample **ins;
    t_sample **outs;
    t_sample **ins_tmp;
    t_sample **outs_tmp;

    int accumSize;
    int ambiFrameSize;
    int pdFrameSize;
    int order;
    int num_sources;
    int nSH;

    int outputIndex;
} t_encoder_tilde;

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
        x->num_sources = sources;
        canvas_update_dsp();
        canvas_resume_dsp(state);
    }
}

// ─────────────────────────────────────
void encoder_tilde_set_source(t_encoder_tilde *x, t_floatarg idx, t_floatarg azi, t_floatarg elev) {
    if (idx < 0 || idx - 1 >= x->num_sources) {
        pd_error(x, "[saf.encoder~] Source index %d out of range (0-%d)", (int)idx,
                 x->num_sources - 1);
        return;
    }
    ambi_enc_setSourceAzi_deg(x->hAmbi, (int)idx - 1, azi);
    ambi_enc_setSourceElev_deg(x->hAmbi, (int)idx - 1, elev);
    ambi_enc_refreshParams(x->hAmbi);
}

// ─────────────────────────────────────
t_int *encoder_tilde_perform(t_int *w) {
    t_encoder_tilde *x = (t_encoder_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n <= x->ambiFrameSize) {
        for (int ch = 0; ch < x->num_sources; ch++) {
            memcpy(x->ins[ch] + x->accumSize, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->accumSize += n;
        if (x->accumSize == x->ambiFrameSize) {
            ambi_enc_process(x->hAmbi, (const float *const *)x->ins, (float *const *)x->outs,
                             x->num_sources, x->nSH, x->ambiFrameSize);
            x->accumSize = 0;
            x->outputIndex = 0;
        }
        for (int ch = 0; ch < x->nSH; ch++) {
            t_sample *out = (t_sample *)(w[3 + x->num_sources + ch]);
            memcpy(out, x->outs[ch] + x->outputIndex, n * sizeof(t_sample));
        }
        x->outputIndex += n;
    } else {
        int chunks = n / x->ambiFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            for (int ch = 0; ch < x->num_sources; ch++) {
                memcpy(x->ins_tmp[ch], (t_sample *)w[3 + ch] + (chunkIndex * x->ambiFrameSize),
                       x->ambiFrameSize * sizeof(t_sample));
            }
            ambi_enc_process(x->hAmbi, (const float *const *)x->ins_tmp,
                             (float *const *)x->outs_tmp, x->num_sources, x->nSH, x->ambiFrameSize);
            for (int ch = 0; ch < x->nSH; ch++) {
                t_sample *out = (t_sample *)(w[3 + x->num_sources + ch]);
                memcpy(out + (chunkIndex * x->ambiFrameSize), x->outs_tmp[ch],
                       x->ambiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 3 + x->num_sources + x->nSH);
}

// ─────────────────────────────────────
void encoder_tilde_dsp(t_encoder_tilde *x, t_signal **sp) {
    // This is a mess. ambi_enc_getFrameSize has fixed frameSize, for encoder is 64 for
    // decoder is 128. In the perform method somethimes I need to accumulate samples sometimes I
    // need to process 2 or more times to avoid change how ambi_enc_ works. I think that in this way
    // is more safe, once that this functions are tested in the main repo. But maybe worse to
    // implement the own set of functions.

    // Set frame sizes and reset indices
    x->ambiFrameSize = ambi_enc_getFrameSize();
    x->pdFrameSize = sp[0]->s_n;
    x->outputIndex = 0;
    x->accumSize = 0;
    int sum = x->num_sources + x->nSH;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    // Initialize the ambisonic encoder
    if (!x->ambi_init) {
        ambi_enc_init(x->hAmbi, sys_getsr());
        ambi_enc_setOutputOrder(x->hAmbi, (SH_ORDERS)x->order);
        ambi_enc_setNumSources(x->hAmbi, x->num_sources);
        if (ambi_enc_getNSHrequired(x->hAmbi) < x->nSH) {
            pd_error(x, "[saf.encoder~] Number of output signals is too low for the %d order.",
                     x->order);
            return;
        }
        x->ambi_init = 1;
    }

    // TODO: Setup multi-out signals
    for (int i = x->num_sources; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    for (int i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }

    x->ins = (t_sample **)getbytes(x->num_sources * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    x->ins_tmp = (t_sample **)getbytes(x->num_sources * sizeof(t_sample *));
    x->outs_tmp = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));

    for (int i = 0; i < x->num_sources; i++) {
        x->ins[i] = (t_sample *)getbytes(x->ambiFrameSize * sizeof(t_sample));
        for (int j = 0; j < x->ambiFrameSize; j++) {
            x->ins[i][j] = 0;
        }
        x->ins_tmp[i] = (t_sample *)getbytes(sp[0]->s_n * sizeof(t_sample));
    }

    for (int i = 0; i < x->nSH; i++) {
        x->outs[i] = (t_sample *)getbytes(x->ambiFrameSize * sizeof(t_sample));
        for (int j = 0; j < x->ambiFrameSize; j++) {
            x->outs[i][j] = 0;
        }
        x->outs_tmp[i] = (t_sample *)getbytes(sp[0]->s_n * sizeof(t_sample));
    }

    dsp_addv(encoder_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void *encoder_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_encoder_tilde *x = (t_encoder_tilde *)pd_new(encoder_tilde_class);
    int order = (argc >= 1) ? atom_getint(argv) : 1;
    int num_sources = (argc >= 2) ? atom_getint(argv + 1) : 1;
    if (argc != 2) {
        pd_error(x, "[saf.encoder~] Wrong number of arguments, use [saf.encoder~ <speakers_count> "
                    "<sources>");
        return NULL;
    }

    order = order < 1 ? 1 : order;
    num_sources = num_sources < 1 ? 1 : num_sources;
    x->ambi_init = 0;

    ambi_enc_create(&x->hAmbi);
    x->order = order;
    x->num_sources = num_sources;
    x->nSH = (order + 1) * (order + 1);
    x->accumSize = 0;

    for (int i = 1; i < x->num_sources; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->nSH; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    return (void *)x;
}

// ─────────────────────────────────────
void encoder_tilde_free(t_encoder_tilde *x) {
    ambi_enc_destroy(&x->hAmbi);
    if (x->ins) {
        for (int i = 0; i < x->num_sources; i++) {
            freebytes(x->ins[i], x->ambiFrameSize * sizeof(t_sample));
            freebytes(x->ins_tmp[i], x->ambiFrameSize * sizeof(t_sample));
        }
        freebytes(x->ins, x->num_sources * sizeof(t_sample *));
        freebytes(x->ins_tmp, x->num_sources * sizeof(t_sample *));
    }

    if (x->outs) {
        for (int i = 0; i < x->nSH; i++) {
            freebytes(x->outs[i], x->ambiFrameSize * sizeof(t_sample));
            freebytes(x->outs_tmp[i], x->ambiFrameSize * sizeof(t_sample));
        }
        freebytes(x->outs, x->nSH * sizeof(t_sample *));
        freebytes(x->outs_tmp, x->nSH * sizeof(t_sample *));
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
