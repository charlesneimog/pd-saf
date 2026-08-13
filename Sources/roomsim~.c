#include <string.h>

#include <m_pd.h>
#include <g_canvas.h>

#include <ambi_roomsim.h>
#include "utilities.h"

static t_class *ambiroom_tilde_class;

// ─────────────────────────────────────
typedef struct _ambi_roomsim {
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

    int nReceivers;
    int nOrder;
    int nIn;
    int nOut;
    int nPreviousIn;
    int nPreviousOut;

    int multichannel;
} t_ambi_roomsim_tilde;

// ─────────────────────────────────────
static void ambiroom_tilde_malloc(t_ambi_roomsim_tilde *x) {
    // Safe free of previous input allocations
    if (x->aIns) {
        for (int i = 0; i < x->nPreviousIn; i++) {
            if (x->aIns[i]) {
                freebytes(x->aIns[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aIns, x->nPreviousIn * sizeof(t_sample *));
        x->aIns = NULL;
    }
    if (x->aInsTmp) {
        for (int i = 0; i < x->nPreviousIn; i++) {
            if (x->aInsTmp[i]) {
                freebytes(x->aInsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aInsTmp, x->nPreviousIn * sizeof(t_sample *));
        x->aInsTmp = NULL;
    }

    // Safe free of previous output allocations
    if (x->aOuts) {
        for (int i = 0; i < x->nPreviousOut; i++) {
            if (x->aOuts[i]) {
                freebytes(x->aOuts[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aOuts, x->nPreviousOut * sizeof(t_sample *));
        x->aOuts = NULL;
    }
    if (x->aOutsTmp) {
        for (int i = 0; i < x->nPreviousOut; i++) {
            if (x->aOutsTmp[i]) {
                freebytes(x->aOutsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aOutsTmp, x->nPreviousOut * sizeof(t_sample *));
        x->aOutsTmp = NULL;
    }

    // New allocations based on current nIn / nOut
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

// ╭─────────────────────────────────────╮
// │                Methods              │
// ╰─────────────────────────────────────╯
static void ambiroom_tilde_set(t_ambi_roomsim_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = s->s_name;

    if (strcmp(method, "source") == 0) {
        if (argc < 4) {
            pd_error(x, "[saf.roomsim~] Use 'source <index> <x> <y> <z>'");
            return;
        }
        int index = atom_getint(argv) - 1;
        if (index < 0 || index >= x->nIn) {
            pd_error(x, "[saf.roomsim~] Source index must be between 1 and %d", x->nIn);
            return;
        }
        float pos_x = atom_getfloat(argv + 1);
        float pos_y = atom_getfloat(argv + 2);
        float pos_z = atom_getfloat(argv + 3);
        ambi_roomsim_setSourceX(x->hAmbi, index, pos_x);
        ambi_roomsim_setSourceY(x->hAmbi, index, pos_y);
        ambi_roomsim_setSourceZ(x->hAmbi, index, pos_z);
    } else if (strcmp(method, "receiver") == 0) {
        if (argc < 4) {
            pd_error(x, "[saf.roomsim~] Use 'receiver <index> <x> <y> <z>'");
            return;
        }
        int index = atom_getint(argv) - 1;
        if (index < 0) {
            pd_error(x, "[saf.roomsim~] Receiver index must be 1 or higher");
            return;
        }

        if (index >= ambi_roomsim_getNumReceivers(x->hAmbi)) {
            pd_error(x, "[saf.roomsim~] Use 'receivers' to set more receivers");
            return;
        }
        float pos_x = atom_getfloat(argv + 1);
        float pos_y = atom_getfloat(argv + 2);
        float pos_z = atom_getfloat(argv + 3);
        ambi_roomsim_setReceiverX(x->hAmbi, index, pos_x);
        ambi_roomsim_setReceiverY(x->hAmbi, index, pos_y);
        ambi_roomsim_setReceiverZ(x->hAmbi, index, pos_z);
    } else if (strcmp(method, "speaker") == 0) {
        float index = atom_getfloat(argv) - 1;
        if (index < 1) {
            pd_error(x, "[saf.roomsim~] Index must be higher than 1");
            return;
        }

        if (index >= ambi_roomsim_getNumReceivers(x->hAmbi)) {
            pd_error(x, "[saf.roomsim~] Use 'receivers' to set more receivers");
            return;
        }

    } else if (strcmp(method, "roomdim") == 0) {
        if (argc < 3) {
            pd_error(x, "[saf.roomsim~] Use 'roomdim <x> <y> <z>'");
            return;
        }
        float x_pos = atom_getfloat(argv);
        float y_pos = atom_getfloat(argv + 1);
        float z_pos = atom_getfloat(argv + 2);
        ambi_roomsim_setRoomDimX(x->hAmbi, x_pos);
        ambi_roomsim_setRoomDimY(x->hAmbi, y_pos);
        ambi_roomsim_setRoomDimZ(x->hAmbi, z_pos);
        x->hAmbiInit = 0;
        canvas_update_dsp();
    } else if (strcmp(method, "reflections") == 0) {
        if (argc < 1) {
            pd_error(x, "[saf.roomsim~] Use 'reflections <0|1>'");
            return;
        }
        int enableIMS = atom_getint(argv);
        ambi_roomsim_setEnableIMSflag(x->hAmbi, enableIMS);
    } else if (strcmp(method, "maxreflectionorder") == 0) {
        if (argc < 1) {
            pd_error(x, "[saf.roomsim~] Use 'maxreflectionorder <order>'");
            return;
        }
        int maxReflectionOrder = atom_getint(argv);
        pd_assert(x, maxReflectionOrder > 0, "[saf.roomsim~] Max reflection order must be > 0");
        if (maxReflectionOrder > 7) {
            logpost(x, 2, "[saf.roomsim~] Numbers higher than 7 is a very high reflection order");
        }
        ambi_roomsim_setMaxReflectionOrder(x->hAmbi, maxReflectionOrder);
    } else if (strcmp(method, "wallabscoeff") == 0) {
        float coeffx_plus = atom_getfloat(argv);
        float coeffx_minus = atom_getfloat(argv + 1);
        float coeffy_plus = atom_getfloat(argv + 2);
        float coeffy_minus = atom_getfloat(argv + 3);
        float coeffz_plus = atom_getfloat(argv + 4);
        float coeffz_minus = atom_getfloat(argv + 5);
        pd_assert(x, coeffx_plus >= 0, "[saf.roomsim~] First value must be positive or 0");
        pd_assert(x, coeffx_minus < 0, "[saf.roomsim~] Second value must be negative");
        pd_assert(x, coeffy_plus >= 0, "[saf.roomsim~] Third value must be positive or 0");
        pd_assert(x, coeffy_minus < 0, "[saf.roomsim~] Fourth value must be negative");
        pd_assert(x, coeffz_plus >= 0, "[saf.roomsim~] Fifth value must be positive or 0");
        pd_assert(x, coeffz_minus < 0, "[saf.roomsim~] Sixth value must be negative");

        ambi_roomsim_setWallAbsCoeff(x->hAmbi, 0, 0, coeffx_plus);
        ambi_roomsim_setWallAbsCoeff(x->hAmbi, 0, 1, coeffx_minus);
        ambi_roomsim_setWallAbsCoeff(x->hAmbi, 1, 0, coeffy_plus);
        ambi_roomsim_setWallAbsCoeff(x->hAmbi, 1, 1, coeffy_minus);
        ambi_roomsim_setWallAbsCoeff(x->hAmbi, 2, 0, coeffz_plus);
        ambi_roomsim_setWallAbsCoeff(x->hAmbi, 2, 1, coeffz_minus);
    } else if (strcmp(method, "normtype") == 0) {
        if (argc < 1) {
            pd_error(x, "[saf.roomsim~] Use 'normtype <1|2|3>'");
            return;
        }
        int normType = atom_getint(argv);
        switch (normType) {
        case NORM_N3D:
            ambi_roomsim_setNormType(x->hAmbi, NORM_N3D);
            break;
        case NORM_SN3D:
            ambi_roomsim_setNormType(x->hAmbi, NORM_SN3D);
            break;
        case NORM_FUMA:
            ambi_roomsim_setNormType(x->hAmbi, NORM_FUMA);
            break;
        default:
            pd_error(x, "[saf.roomsim~] Unknown normtype: %s", method);
        }
    } else if (strcmp(method, "receivers") == 0) {
        float num_receivers = atom_getfloat(argv);
        ambi_roomsim_setNumReceivers(x->hAmbi, num_receivers);
    } else {
        pd_error(x, "[saf.roomsim~] Unknown method %s", method);
    }
}

// ─────────────────────────────────────
t_int *ambiroom_tilde_performmultichannel(t_int *w) {
    t_ambi_roomsim_tilde *x = (t_ambi_roomsim_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, ins + (n * ch), n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_roomsim_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            for (int ch = 0; ch < x->nIn; ch++) {
                memcpy(x->aInsTmp[ch], (t_sample *)w[3] + ch * n + chunkIndex * x->nAmbiFrameSize,
                       x->nAmbiFrameSize * sizeof(t_sample));
            }

            ambi_roomsim_process(x->hAmbi, (const float *const *)x->aInsTmp,
                                 (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nAmbiFrameSize);

            for (int ch = 0; ch < x->nOut; ch++) {
                memcpy(outs + ch * n + chunkIndex * x->nAmbiFrameSize, x->aOutsTmp[ch],
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 5);
}

// ─────────────────────────────────────
t_int *ambiroom_tilde_perform(t_int *w) {
    t_ambi_roomsim_tilde *x = (t_ambi_roomsim_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_roomsim_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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

            ambi_roomsim_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
void ambiroom_tilde_dsp(t_ambi_roomsim_tilde *x, t_signal **sp) {
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;

    x->nIn = x->multichannel ? sp[0]->s_nchans : x->nIn;
    if (x->nOrder < 1) {
        x->nOrder = 1;
    }

    int sum = x->nIn + x->nOut;
    int sigvecsize = sum + 2;

    x->nAmbiFrameSize = ambi_roomsim_getFrameSize();

    if (!x->hAmbiInit) {
        ambi_roomsim_init(x->hAmbi, sys_getsr());
        ambi_roomsim_setOutputOrder(x->hAmbi, (SH_ORDERS)x->nOrder);
        if (ambi_roomsim_getNSHrequired(x->hAmbi) < x->nOut) {
            pd_error(x, "[saf.roomsim~] Number of output signals is too low for the %d order.",
                     x->nOrder);
            return;
        }
        x->hAmbiInit = 1;
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        ambi_roomsim_setNumSources(x->hAmbi, x->nIn);
        ambiroom_tilde_malloc(x);
    }

    if (sp[0]->s_nchans > 1 && !x->multichannel) {
        pd_error(x, "Multichannel mode is off, but input is multichannel, use '-m' flag");
    }

    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(ambiroom_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
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
        dsp_addv(ambiroom_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *ambiroom_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    if (argc < 2) {
        pd_error(NULL, "[saf.roomsim~] Wrong number of arguments, use [saf.roomsim~ "
                       "<num_sources> <ambisonic_order>] or [saf.roomsim~ -m <ambisonic_order>] "
                       "for multichannel input");
        return NULL;
    }

    t_ambi_roomsim_tilde *x = (t_ambi_roomsim_tilde *)pd_new(ambiroom_tilde_class);
    int order = 1;
    int num_sources = 4;

    if (argv[0].a_type == A_SYMBOL) {
        if (strcmp(atom_getsymbol(argv)->s_name, "-m") != 0) {
            pd_error(x, "[saf.roomsim~] Expected '-m' in second argument.");
            return NULL;
        }
        order = (argc >= 2) ? atom_getint(argv + 1) : 1;
        x->multichannel = 1;
    } else {
        num_sources = (argc >= 1) ? atom_getint(argv) : 1;
        order = (argc >= 2) ? atom_getint(argv + 1) : 1;
        x->multichannel = 0;
    }

    x->hAmbiInit = 0;
    x->nOrder = order;
    x->nIn = num_sources;
    x->nOut = (order + 1) * (order + 1);
    x->nInAccIndex = 0;
    x->nReceivers = 1;

    ambi_roomsim_create(&x->hAmbi);
    ambi_roomsim_setEnableIMSflag(x->hAmbi, 0);
    ambi_roomsim_setNumReceivers(x->hAmbi, x->nReceivers);
    ambi_roomsim_setOutputOrder(x->hAmbi, x->nOrder);
    ambi_roomsim_setNormType(x->hAmbi, NORM_N3D);

    ambi_roomsim_setReceiverX(x->hAmbi, 0, 2.5);
    ambi_roomsim_setReceiverY(x->hAmbi, 0, 2.5);
    ambi_roomsim_setReceiverZ(x->hAmbi, 0, 2.5);

    ambi_roomsim_setRoomDimX(x->hAmbi, 5);
    ambi_roomsim_setRoomDimY(x->hAmbi, 5);
    ambi_roomsim_setRoomDimZ(x->hAmbi, 5);

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
void ambiroom_tilde_free(t_ambi_roomsim_tilde *x) {
    ambi_roomsim_destroy(&x->hAmbi);

    if (x->aIns) {
        for (int i = 0; i < x->nPreviousIn; i++) {
            if (x->aIns[i]) {
                freebytes(x->aIns[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aIns, x->nPreviousIn * sizeof(t_sample *));
    }
    if (x->aInsTmp) {
        for (int i = 0; i < x->nPreviousIn; i++) {
            if (x->aInsTmp[i]) {
                freebytes(x->aInsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aInsTmp, x->nPreviousIn * sizeof(t_sample *));
    }
    if (x->aOuts) {
        for (int i = 0; i < x->nPreviousOut; i++) {
            if (x->aOuts[i]) {
                freebytes(x->aOuts[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aOuts, x->nPreviousOut * sizeof(t_sample *));
    }
    if (x->aOutsTmp) {
        for (int i = 0; i < x->nPreviousOut; i++) {
            if (x->aOutsTmp[i]) {
                freebytes(x->aOutsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aOutsTmp, x->nPreviousOut * sizeof(t_sample *));
    }
}

// ─────────────────────────────────────
// clang-format off
void setup_saf0x2eroomsim_tilde(void) {
    ambiroom_tilde_class = class_new(gensym("saf.roomsim~"), (t_newmethod)ambiroom_tilde_new,
                                     (t_method)ambiroom_tilde_free, sizeof(t_ambi_roomsim_tilde),
                                     CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(ambiroom_tilde_class, t_ambi_roomsim_tilde, sample);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_dsp, gensym("dsp"), A_CANT, 0);

    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("source"), A_GIMME, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("roomdim"), A_GIMME, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("receiver"), A_GIMME, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("reflections"), A_GIMME, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("maxreflectionorder"), A_GIMME, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("wallabscoeff"), A_GIMME, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("normtype"), A_GIMME, 0);

    //
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("receivers"), A_GIMME, 0);
}
