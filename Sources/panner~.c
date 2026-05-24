#include <string.h>
#include <stdlib.h>

#include <m_pd.h>
#include <g_canvas.h>

#include "utilities.h"
#include <panner.h>

static t_class *panner_tilde_class;

// ─────────────────────────────────────
typedef struct _panner_tilde {
    t_object obj;
    t_sample sample;

    void *hAmbi;
    unsigned hAmbiInit;

    // Internal SAF buffers (strictly floats)
    float **saf_ins;
    float **saf_outs;

    // Pd arrays to hold inlet/outlet pointers
    t_sample **pd_ins;
    t_sample **pd_outs;

    int nAmbiFrameSize;
    int nPdFrameSize;
    int nAccIndex;

    int nOrder;
    int nIn;
    int nOut;

    int multichannel;
} t_panner_tilde;

// ─────────────────────────────────────
static void panner_tilde_realloc(t_panner_tilde *x, int new_nIn, int new_nOut) {
    if (x->nIn == new_nIn && x->nOut == new_nOut && x->saf_ins != NULL) {
        return;
    }

    if (x->saf_ins) {
        for (int i = 0; i < x->nIn; i++) {
            freebytes(x->saf_ins[i], x->nAmbiFrameSize * sizeof(float));
        }
        freebytes(x->saf_ins, x->nIn * sizeof(float *));
    }
    if (x->saf_outs) {
        for (int i = 0; i < x->nOut; i++) {
            freebytes(x->saf_outs[i], x->nAmbiFrameSize * sizeof(float));
        }
        freebytes(x->saf_outs, x->nOut * sizeof(float *));
    }
    if (x->pd_ins) {
        freebytes(x->pd_ins, x->nIn * sizeof(t_sample *));
    }
    if (x->pd_outs) {
        freebytes(x->pd_outs, x->nOut * sizeof(t_sample *));
    }

    x->nIn = new_nIn;
    x->nOut = new_nOut;

    if (x->nIn > 0) {
        x->saf_ins = (float **)getbytes(x->nIn * sizeof(float *));
        x->pd_ins = (t_sample **)getbytes(x->nIn * sizeof(t_sample *));
        for (int i = 0; i < x->nIn; i++) {
            x->saf_ins[i] = (float *)getbytes(x->nAmbiFrameSize * sizeof(float));
            memset(x->saf_ins[i], 0, x->nAmbiFrameSize * sizeof(float));
        }
    } else {
        x->saf_ins = NULL;
        x->pd_ins = NULL;
    }

    if (x->nOut > 0) {
        x->saf_outs = (float **)getbytes(x->nOut * sizeof(float *));
        x->pd_outs = (t_sample **)getbytes(x->nOut * sizeof(t_sample *));
        for (int i = 0; i < x->nOut; i++) {
            x->saf_outs[i] = (float *)getbytes(x->nAmbiFrameSize * sizeof(float));
            memset(x->saf_outs[i], 0, x->nAmbiFrameSize * sizeof(float));
        }
    } else {
        x->saf_outs = NULL;
        x->pd_outs = NULL;
    }

    x->nAccIndex = 0;
}

// ─────────────────────────────────────
static void panner_tilde_set(t_panner_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = s->s_name;
    if (strcmp(method, "source") == 0 && argc >= 3) {
        int index = atom_getint(argv) - 1;
        float azi = atom_getfloat(argv + 1);
        float ele = atom_getfloat(argv + 2);
        panner_setSourceAzi_deg(x->hAmbi, index, azi);
        panner_setSourceElev_deg(x->hAmbi, index, ele);
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "speaker") == 0 && argc >= 3) {
        int index = atom_getint(argv) - 1;
        float azi = atom_getfloat(argv + 1);
        float ele = atom_getfloat(argv + 2);
        panner_setLoudspeakerAzi_deg(x->hAmbi, index, azi);
        panner_setLoudspeakerElev_deg(x->hAmbi, index, ele);
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "dtt") == 0 && argc >= 1) {
        float dtt = atom_getfloat(argv);
        if (dtt > 1.0f || dtt < 0.0f) {
            pd_error(x, "[saf.panner~] dtt must be between 0 and 1");
            return;
        }
        panner_setDTT(x->hAmbi, dtt);
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "spread") == 0 && argc >= 1) {
        float spread = atom_getfloat(argv);
        panner_setSpread(x->hAmbi, spread);
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "yaw") == 0 && argc >= 1) {
        panner_setYaw(x->hAmbi, atom_getfloat(argv));
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "pitch") == 0 && argc >= 1) {
        panner_setPitch(x->hAmbi, atom_getfloat(argv));
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "roll") == 0 && argc >= 1) {
        panner_setRoll(x->hAmbi, atom_getfloat(argv));
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "flipYaw") == 0 && argc >= 1) {
        panner_setFlipYaw(x->hAmbi, atom_getint(argv));
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "flipPitch") == 0 && argc >= 1) {
        panner_setFlipPitch(x->hAmbi, atom_getint(argv));
        panner_refreshSettings(x->hAmbi);
    } else if (strcmp(method, "flipRoll") == 0 && argc >= 1) {
        panner_setFlipRoll(x->hAmbi, atom_getint(argv));
        panner_refreshSettings(x->hAmbi);
    }
}

// ─────────────────────────────────────
t_int *panner_tilde_performmultichannel(t_int *w) {
    t_panner_tilde *x = (t_panner_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    int nAmbi = x->nAmbiFrameSize;
    int idx = 0;

    while (idx < n) {
        int to_process = nAmbi - x->nAccIndex;
        if (to_process > n - idx) {
            to_process = n - idx;
        }

        // Copy input chunks (t_sample to float)
        for (int ch = 0; ch < x->nIn; ch++) {
            for (int i = 0; i < to_process; i++) {
                x->saf_ins[ch][x->nAccIndex + i] = (float)(ins[(ch * n) + idx + i]);
            }
        }

        // Copy output chunks from previously processed buffer (float to t_sample)
        for (int ch = 0; ch < x->nOut; ch++) {
            for (int i = 0; i < to_process; i++) {
                outs[(ch * n) + idx + i] = (t_sample)(x->saf_outs[ch][x->nAccIndex + i]);
            }
        }

        x->nAccIndex += to_process;
        idx += to_process;

        // Process block when buffer is full
        if (x->nAccIndex == nAmbi) {
            if (x->nIn > 0 && x->nOut > 0) {
                panner_initCodec(x->hAmbi);
                panner_process(x->hAmbi, (const float *const *)x->saf_ins,
                               (float *const *)x->saf_outs, x->nIn, x->nOut, nAmbi);
            } else {
                for (int ch = 0; ch < x->nOut; ch++) {
                    memset(x->saf_outs[ch], 0, nAmbi * sizeof(float));
                }
            }
            x->nAccIndex = 0;
        }
    }

    return (w + 5);
}

// ─────────────────────────────────────
t_int *panner_tilde_perform(t_int *w) {
    t_panner_tilde *x = (t_panner_tilde *)(w[1]);
    int n = (int)(w[2]);

    for (int i = 0; i < x->nIn; i++) {
        x->pd_ins[i] = (t_sample *)w[3 + i];
    }
    for (int i = 0; i < x->nOut; i++) {
        x->pd_outs[i] = (t_sample *)w[3 + x->nIn + i];
    }

    int nAmbi = x->nAmbiFrameSize;
    int idx = 0;

    while (idx < n) {
        int to_process = nAmbi - x->nAccIndex;
        if (to_process > n - idx) {
            to_process = n - idx;
        }

        // Copy input chunks (t_sample to float)
        for (int ch = 0; ch < x->nIn; ch++) {
            for (int i = 0; i < to_process; i++) {
                x->saf_ins[ch][x->nAccIndex + i] = (float)(x->pd_ins[ch][idx + i]);
            }
        }

        // Copy output chunks from previously processed buffer (float to t_sample)
        for (int ch = 0; ch < x->nOut; ch++) {
            for (int i = 0; i < to_process; i++) {
                x->pd_outs[ch][idx + i] = (t_sample)(x->saf_outs[ch][x->nAccIndex + i]);
            }
        }

        x->nAccIndex += to_process;
        idx += to_process;

        // Process block when buffer is full
        if (x->nAccIndex == nAmbi) {
            if (x->nIn > 0 && x->nOut > 0) {
                panner_initCodec(x->hAmbi);
                panner_process(x->hAmbi, (const float *const *)x->saf_ins,
                               (float *const *)x->saf_outs, x->nIn, x->nOut, nAmbi);
            } else {
                for (int ch = 0; ch < x->nOut; ch++) {
                    memset(x->saf_outs[ch], 0, nAmbi * sizeof(float));
                }
            }
            x->nAccIndex = 0;
        }
    }

    return (w + 3 + x->nIn + x->nOut);
}

// ─────────────────────────────────────
void panner_tilde_dsp(t_panner_tilde *x, t_signal **sp) {
    x->nPdFrameSize = sp[0]->s_n;

    int new_nIn = x->multichannel ? sp[0]->s_nchans : x->nIn;
    int new_nOut = x->nOut;

    if (new_nIn != x->nIn || new_nOut != x->nOut) {
        panner_tilde_realloc(x, new_nIn, new_nOut);

        panner_setNumSources(x->hAmbi, x->nIn);
        for (int i = 0; i < x->nIn; i++) {
            float azi = 360.0f / (float)x->nIn * i;
            panner_setSourceAzi_deg(x->hAmbi, i, azi);
            panner_setSourceElev_deg(x->hAmbi, i, 0);
        }
        panner_refreshSettings(x->hAmbi);
    }

    panner_setNumSources(x->hAmbi, x->nIn);
    panner_setNumLoudspeakers(x->hAmbi, x->nOut);

    if (!x->hAmbiInit || panner_getCodecStatus(x->hAmbi) == CODEC_STATUS_NOT_INITIALISED) {
        panner_init(x->hAmbi, sp[0]->s_sr > 0 ? sp[0]->s_sr : 44100);
        panner_initCodec(x->hAmbi);
        x->hAmbiInit = 1;
    }

    if (sp[0]->s_nchans > 1 && !x->multichannel) {
        pd_error(x, "Multichannel mode is off, but input is multichannel, use '-m' flag");
    }

    if (x->multichannel) {
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(panner_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
    } else {
        for (int i = x->nIn; i < x->nIn + x->nOut; i++) {
            signal_setmultiout(&sp[i], 1);
        }
        int sigvecsize = x->nIn + x->nOut + 3;
        t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));
        sigvec[0] = (t_int)x;
        sigvec[1] = (t_int)sp[0]->s_n;
        for (int i = 0; i < x->nIn + x->nOut; i++) {
            sigvec[2 + i] = (t_int)sp[i]->s_vec;
        }
        dsp_addv(panner_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *panner_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    if (argc < 1) {
        pd_error(NULL, "[saf.panner~] Wrong number of arguments, use [saf.panner~ "
                       "<num_sources> <num_speakers>] or [saf.panner~ -m <num_speakers>] "
                       "for multichannel input");
        return NULL;
    }

    t_panner_tilde *x = (t_panner_tilde *)pd_new(panner_tilde_class);
    int num_sources = 4;
    int num_speakers = 1;

    x->multichannel = 0;

    if (argv[0].a_type == A_SYMBOL) {
        if (strcmp(atom_getsymbol(argv)->s_name, "-m") != 0) {
            pd_error(x, "[saf.panner~] Expected '-m' in first argument for multichannel.");
            return NULL;
        }
        x->multichannel = 1;
        num_speakers = (argc >= 2) ? atom_getint(argv + 1) : 1;
        num_sources = 1;
    } else {
        num_sources = (argc >= 1) ? atom_getint(argv) : 4;
        num_speakers = (argc >= 2) ? atom_getint(argv + 1) : 1;
    }

    x->saf_ins = NULL;
    x->saf_outs = NULL;
    x->pd_ins = NULL;
    x->pd_outs = NULL;
    x->nIn = 0;
    x->nOut = 0;

    panner_create(&x->hAmbi);
    int sr = sys_getsr();
    panner_init(x->hAmbi, sr > 0 ? sr : 44100);
    x->hAmbiInit = 0;

    x->nAmbiFrameSize = panner_getFrameSize();
    if (x->nAmbiFrameSize <= 0) {
        x->nAmbiFrameSize = 64; // Fallback safeguard
    }

    panner_tilde_realloc(x, num_sources, num_speakers);

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

    return x;
}

// ─────────────────────────────────────
void panner_tilde_free(t_panner_tilde *x) {
    panner_destroy(&x->hAmbi);

    if (x->saf_ins) {
        for (int i = 0; i < x->nIn; i++) {
            freebytes(x->saf_ins[i], x->nAmbiFrameSize * sizeof(float));
        }
        freebytes(x->saf_ins, x->nIn * sizeof(float *));
    }
    if (x->saf_outs) {
        for (int i = 0; i < x->nOut; i++) {
            freebytes(x->saf_outs[i], x->nAmbiFrameSize * sizeof(float));
        }
        freebytes(x->saf_outs, x->nOut * sizeof(float *));
    }
    if (x->pd_ins) {
        freebytes(x->pd_ins, x->nIn * sizeof(t_sample *));
    }
    if (x->pd_outs) {
        freebytes(x->pd_outs, x->nOut * sizeof(t_sample *));
    }
}

// ─────────────────────────────────────
// clang-format off
void setup_saf0x2epanner_tilde(void) {
    panner_tilde_class =
        class_new(gensym("saf.panner~"), (t_newmethod)panner_tilde_new, (t_method)panner_tilde_free,
                  sizeof(t_panner_tilde), CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(panner_tilde_class, t_panner_tilde, sample);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_dsp, gensym("dsp"), A_CANT, 0);

    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("source"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("speaker"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("dtt"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("spread"), A_GIMME, 0);
    
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("yaw"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("pitch"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("roll"), A_GIMME, 0);
    
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("flipYaw"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("flipPitch"), A_GIMME, 0);
    class_addmethod(panner_tilde_class, (t_method)panner_tilde_set, gensym("flipRoll"), A_GIMME, 0);
}
