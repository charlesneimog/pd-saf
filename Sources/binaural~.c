#include <ambi_bin.h>
#include <m_pd.h>

static t_class *binaural_tilde_class;

// ─────────────────────────────────────
typedef struct _binaural_tilde {
    t_object obj;
    void *hAmbi;
    t_sample sample;

    t_sample **ins;
    t_sample **outs;

    int order;
    int nSH;
} t_binaural_tilde;

// DSP Perform Routine
// ─────────────────────────────────────
t_int *binaural_tilde_perform(t_int *w) {
    t_binaural_tilde *x = (t_binaural_tilde *)(w[1]);
    int n = (int)(w[2]);

    // Inputs (Ambisonic channels)
    for (int i = 0; i < x->nSH; i++) {
        x->ins[i] = (t_sample *)(w[3 + i]);
    }

    // Outputs (binaural ears)
    for (int i = 0; i < 2; i++) {
        x->outs[i] = (t_sample *)(w[3 + x->nSH + i]);
    }

    ambi_bin_process(x->hAmbi, (const float *const *)x->ins, x->outs, x->nSH, 2, n);

    return (w + 3 + x->nSH + 2);
}

// DSP Setup
// ─────────────────────────────────────
void binaural_tilde_dsp(t_binaural_tilde *x, t_signal **sp) {
    int i;
    int sum = x->nSH + 2;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    ambi_bin_init(x->hAmbi, sys_getsr());
    ambi_bin_initCodec(x->hAmbi);

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;

    for (i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }

    if (x->ins)
        freebytes(x->ins, x->nSH * sizeof(t_sample *));
    if (x->outs)
        freebytes(x->outs, 2 * sizeof(t_sample *));

    x->ins = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(2 * sizeof(t_sample *));

    dsp_addv(binaural_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// Set Listener Orientation
// ─────────────────────────────────────
void binaural_tilde_set_yaw(t_binaural_tilde *x, t_floatarg yaw) {
    ambi_bin_setYaw(x->hAmbi, (float)yaw);
}

// ─────────────────────────────────────
void binaural_tilde_set_pitch(t_binaural_tilde *x, t_floatarg pitch) {
    ambi_bin_setPitch(x->hAmbi, (float)pitch);
}

// Constructor
// ─────────────────────────────────────
void *binaural_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_binaural_tilde *x = (t_binaural_tilde *)pd_new(binaural_tilde_class);
    int order = 1;

    if (argc >= 1) {
        order = atom_getint(argv);
    }
    order = order < 0 ? 0 : order;
    x->order = order;
    x->nSH = (order + 1) * (order + 1);

    ambi_bin_create(&x->hAmbi);
    ambi_bin_setInputOrderPreset(x->hAmbi, (SH_ORDERS)order);
    ambi_bin_setNormType(x->hAmbi, NORM_N3D);
    ambi_bin_setEnableRotation(x->hAmbi, 1);

    // Create inlets for Ambisonic channels
    for (int i = 0; i < x->nSH; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    // Create outlets for binaural output
    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);

    return (void *)x;
}

// Destructor
// ─────────────────────────────────────
void binaural_tilde_free(t_binaural_tilde *x) {
    if (x->ins)
        freebytes(x->ins, x->nSH * sizeof(t_sample *));
    if (x->outs)
        freebytes(x->outs, 2 * sizeof(t_sample *));
    ambi_bin_destroy(&x->hAmbi);
}

// ─────────────────────────────────────
void setup_saf0x2ebinaural_tilde(void) {
    binaural_tilde_class = class_new(gensym("saf.binaural~"), (t_newmethod)binaural_tilde_new,
                                     (t_method)binaural_tilde_free, sizeof(t_binaural_tilde),
                                     CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(binaural_tilde_class, t_binaural_tilde, sample);
    class_addmethod(binaural_tilde_class, (t_method)binaural_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(binaural_tilde_class, (t_method)binaural_tilde_set_yaw, gensym("yaw"), A_FLOAT,
                    0);
    class_addmethod(binaural_tilde_class, (t_method)binaural_tilde_set_pitch, gensym("pitch"),
                    A_FLOAT, 0);
}
