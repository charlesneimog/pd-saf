#include <ambi_enc.h>
#include <m_pd.h>

static t_class *encoder_tilde_class;

// ─────────────────────────────────────
typedef struct _binaural_tilde {
    t_object obj;
    void *hAmbi;
    t_sample sample;

    t_sample **ins;
    t_sample **outs;

    int order;
    int num_sources;
    int nSH;
} t_encoder_tilde;

// ─────────────────────────────────────
t_int *encoder_tilde_perform(t_int *w) {
    t_encoder_tilde *x = (t_encoder_tilde *)(w[1]);
    int n = (int)(w[2]);

    for (int i = 0; i < x->num_sources; i++) {
        x->ins[i] = (t_sample *)(w[3 + i]);
    }

    for (int i = 0; i < x->nSH; i++) {
        x->outs[i] = (t_sample *)(w[3 + x->num_sources + i]);
    }

    ambi_enc_process(x->hAmbi, (const float *const *)x->ins, x->outs, x->num_sources, x->nSH, n);

    return (w + 3 + x->num_sources + x->nSH);
}

// ─────────────────────────────────────
void encoder_tilde_dsp(t_encoder_tilde *x, t_signal **sp) {
    int i;

    int sum = x->num_sources + x->nSH;
    int sigvecsize = sum + 2; // +1 for x, +1 for blocksize
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));
    ambi_enc_init(x->hAmbi, sys_getsr());

    for (int i = x->num_sources; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    for (i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }
    if (x->ins) {
        freebytes(x->ins, x->num_sources * sizeof(t_sample *));
    }
    if (x->outs) {
        freebytes(x->outs, x->nSH * sizeof(t_sample *));
    }

    x->ins = (t_sample **)getbytes(x->num_sources * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));

    dsp_addv(encoder_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void encoder_tilde_set_source(t_encoder_tilde *x, t_floatarg idx, t_floatarg azi, t_floatarg elev) {
    if (idx < 0 || idx >= x->num_sources) {
        pd_error(x, "Source index %d out of range (0-%d)", (int)idx, x->num_sources - 1);
        return;
    }
    ambi_enc_setSourceAzi_deg(x->hAmbi, (int)idx, azi);
    ambi_enc_setSourceElev_deg(x->hAmbi, (int)idx, elev);
    // ambi_enc_initCodec(x->hAmbi);
}

/* Constructor */
// ─────────────────────────────────────
void *encoder_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_encoder_tilde *x = (t_encoder_tilde *)pd_new(encoder_tilde_class);
    int order = 1;
    int num_sources = 1;

    // Parse arguments: [order] [num_sources]
    if (argc >= 1) {
        order = atom_getint(argv);
    }
    if (argc >= 2) {
        num_sources = atom_getint(argv + 1);
    }
    order = order < 0 ? 0 : order;
    num_sources = num_sources < 1 ? 1 : num_sources;

    // Initialize ambi_enc
    ambi_enc_create(&x->hAmbi);
    x->order = order;
    x->num_sources = num_sources;
    x->nSH = (order + 1) * (order + 1);

    // Default configuration
    ambi_enc_setOutputOrder(x->hAmbi, (SH_ORDERS)order);
    ambi_enc_setNormType(x->hAmbi, NORM_N3D);
    ambi_enc_setEnablePostScaling(x->hAmbi, 0);
    ambi_enc_setNumSources(x->hAmbi, num_sources);

    for (int i = 1; i < x->num_sources; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    // Create outlets for each Ambisonic channel
    for (int i = 0; i < x->nSH; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    return (void *)x;
}

// ─────────────────────────────────────
void encoder_tilde_free(t_encoder_tilde *x) {
    if (x->ins) {
        freebytes(x->ins, x->num_sources * sizeof(t_sample *));
    }
    if (x->outs) {
        freebytes(x->outs, x->nSH * sizeof(t_sample *));
    }

    ambi_enc_destroy(&x->hAmbi);
}

// ─────────────────────────────────────
void setup_saf0x2eencoder_tilde(void) {
    encoder_tilde_class = class_new(gensym("saf.encoder~"), (t_newmethod)encoder_tilde_new,
                                    (t_method)encoder_tilde_free, sizeof(t_encoder_tilde),
                                    CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(encoder_tilde_class, t_encoder_tilde, sample);
    class_addmethod(encoder_tilde_class, (t_method)encoder_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(encoder_tilde_class, (t_method)encoder_tilde_set_source, gensym("source"),
                    A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    // class_addmethod(encoder_tilde_class, (t_method)encoder_tilde_set_norm,
    //                 gensym("norm"), A_DEFFLOAT, 0);
    // class_addmethod(encoder_tilde_class,
    //                 (t_method)encoder_tilde_set_post_scaling,
    //                 gensym("post_scaling"), A_DEFFLOAT, 0);
}
