#include <ambi_dec.h>
#include <m_pd.h>
#include <stdlib.h>

static t_class *decoder_tilde_class;
#define ORDER2NSH(order) ((order + 1) * (order + 1))

// ─────────────────────────────────────
typedef struct _decoder_tilde {
    t_object obj;
    void *hAmbi;
    t_sample sample;

    t_sample **ins;
    t_sample **outs;

    int order;
    int nSH;
    int num_loudspeakers;
} t_decoder_tilde;

// ─────────────────────────────────────
t_int *decoder_tilde_perform(t_int *w) {
    t_decoder_tilde *x = (t_decoder_tilde *)(w[1]);
    int n = (int)(w[2]);
    for (int i = 0; i < x->nSH; i++) {
        x->ins[i] = (t_sample *)(w[3 + i]);
    }
    for (int i = 0; i < x->num_loudspeakers; i++) {
        x->outs[i] = (t_sample *)(w[3 + x->nSH + i]);
    }

    ambi_dec_process(x->hAmbi, (const float *const *)x->ins, x->outs, x->nSH,
                     x->num_loudspeakers, n);

    return (w + 3 + x->nSH + x->num_loudspeakers);
}

// ─────────────────────────────────────
void decoder_tilde_dsp(t_decoder_tilde *x, t_signal **sp) {
    int i;
    int sum = x->nSH + x->num_loudspeakers;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    //
    ambi_dec_init(x->hAmbi, sp[0]->s_sr);

    // getRSH(order, (float *)direction_deg, 1, y);
    // cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, nSH, signalLength, 1, 1.0f, y, 1,
    // inSig,
    //             signalLength, 0.0f, FLATTEN2D(shSig), signalLength);

    for (i = x->nSH; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    for (i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }

    if (x->ins) {
        freebytes(x->ins, x->nSH * sizeof(t_sample *));
    }
    if (x->outs) {
        freebytes(x->outs, x->num_loudspeakers * sizeof(t_sample *));
    }
    x->ins = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(x->num_loudspeakers * sizeof(t_sample *));

    dsp_addv(decoder_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void *decoder_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_decoder_tilde *x = (t_decoder_tilde *)pd_new(decoder_tilde_class);
    int order = 1;
    int num_loudspeakers = 2;

    if (argc >= 1) {
        order = atom_getint(argv);
    }
    if (argc >= 2) {
        num_loudspeakers = atom_getint(argv + 1);
    }

    order = order < 0 ? 0 : order;
    num_loudspeakers = num_loudspeakers < 1 ? 1 : num_loudspeakers;

    x->order = order;
    x->nSH = (order + 1) * (order + 1);
    x->num_loudspeakers = num_loudspeakers;

    // Create the Ambisonic decoder instance.
    ambi_dec_create(&x->hAmbi);
    ambi_dec_setNormType(x->hAmbi, NORM_N3D);
    ambi_dec_setMasterDecOrder(x->hAmbi, ORDER2NSH(order));

    logpost(x, 3, "SAF decoder~: %d SH, %d loudspeakers", x->nSH, x->num_loudspeakers);
    if (x->num_loudspeakers == 2) {
        ambi_dec_setOutputConfigPreset(x->hAmbi, LOUDSPEAKER_ARRAY_PRESET_STEREO);
    } else if (x->num_loudspeakers == 22) {
        ambi_dec_setOutputConfigPreset(x->hAmbi, LOUDSPEAKER_ARRAY_PRESET_22PX);
    }

    // Add other presets as needed
    ambi_dec_setDecMethod(x->hAmbi, DECODING_METHOD_SAD, 0);
    ambi_dec_setDecMethod(x->hAmbi, DECODING_METHOD_SAD, 1);

    for (int i = 1; i < x->nSH; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->num_loudspeakers; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    x->ins = NULL;
    x->outs = NULL;

    return (void *)x;
}

// ─────────────────────────────────────
void decoder_tilde_free(t_decoder_tilde *x) {
    if (x->ins) {
        freebytes(x->ins, x->nSH * sizeof(t_sample *));
    }
    if (x->outs) {
        freebytes(x->outs, x->num_loudspeakers * sizeof(t_sample *));
    }

    ambi_dec_destroy(&x->hAmbi);
}

// ─────────────────────────────────────
void setup_saf0x2edecoder_tilde(void) {
    decoder_tilde_class = class_new(gensym("saf.decoder~"), (t_newmethod)decoder_tilde_new,
                                    (t_method)decoder_tilde_free, sizeof(t_decoder_tilde),
                                    CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(decoder_tilde_class, t_decoder_tilde, sample);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
