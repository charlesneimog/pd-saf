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

    int nOrder;
    int nIn;
    int nOut;
    int nPreviousIn;
    int nPreviousOut;

    int multichannel;
} t_ambi_roomsim_tilde;

// ─────────────────────────────────────
static void ambiroom_tilde_malloc(t_ambi_roomsim_tilde *x) {
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
static void ambiroom_tilde_set(t_ambi_roomsim_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;

    // Positions
    if (strcmp(method, "sourcex") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float xCoord = atom_getfloat(argv + 2);
        ambi_roomsim_setSourceX(x->hAmbi, index, xCoord);
    } else if (strcmp(method, "sourcey") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float yCoord = atom_getfloat(argv + 2);
        ambi_roomsim_setSourceY(x->hAmbi, index, yCoord);
    } else if (strcmp(method, "sourcez") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float zCoord = atom_getfloat(argv + 2);
        ambi_roomsim_setSourceZ(x->hAmbi, index, zCoord);
    } else if (strcmp(method, "source") == 0) {
        float index = atom_getfloat(argv + 1) - 1;
        float pos_x = atom_getfloat(argv + 2);
        float pos_y = atom_getfloat(argv + 3);
        float pos_z = atom_getfloat(argv + 4);
        ambi_roomsim_setSourceX(x->hAmbi, index, pos_x);
        ambi_roomsim_setSourceY(x->hAmbi, index, pos_y);
        ambi_roomsim_setSourceZ(x->hAmbi, index, pos_z);
    }

    else if (strcmp(method, "receiverx") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float xCoord = atom_getfloat(argv + 2);
        ambi_roomsim_setReceiverX(x->hAmbi, index, xCoord);
    } else if (strcmp(method, "receivery") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float yCoord = atom_getfloat(argv + 2);
        ambi_roomsim_setReceiverY(x->hAmbi, index, yCoord);
    } else if (strcmp(method, "receiverz") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float zCoord = atom_getfloat(argv + 2);
        ambi_roomsim_setReceiverZ(x->hAmbi, index, zCoord);
    } else if (strcmp(method, "receiver") == 0) {
        float index = atom_getfloat(argv + 1) - 1;
        float pos_x = atom_getfloat(argv + 2);
        float pos_y = atom_getfloat(argv + 3);
        float pos_z = atom_getfloat(argv + 4);
        ambi_roomsim_setReceiverX(x->hAmbi, index, pos_x);
        ambi_roomsim_setReceiverY(x->hAmbi, index, pos_y);
        ambi_roomsim_setReceiverZ(x->hAmbi, index, pos_z);
    }

    else if (strcmp(method, "roomdimx") == 0) {
        float roomDimX = atom_getfloat(argv + 1) - 1;
        ambi_roomsim_setRoomDimX(x->hAmbi, roomDimX);
    } else if (strcmp(method, "roomdimy") == 0) {
        float roomDimY = atom_getfloat(argv + 1) - 1;
        ambi_roomsim_setRoomDimY(x->hAmbi, roomDimY);
    } else if (strcmp(method, "roomdimz") == 0) {
        float roomDimZ = atom_getfloat(argv + 1) - 1;
        ambi_roomsim_setRoomDimZ(x->hAmbi, roomDimZ);
    } else if (strcmp(method, "room") == 0) {
        // set room  <x> <y> <z>
        float pos_x = atom_getfloat(argv + 1);
        float pos_y = atom_getfloat(argv + 2);
        float pos_z = atom_getfloat(argv + 3);
        ambi_roomsim_setRoomDimX(x->hAmbi, pos_x);
        ambi_roomsim_setRoomDimY(x->hAmbi, pos_y);
        ambi_roomsim_setRoomDimZ(x->hAmbi, pos_z);
    }

    // Config
    else if (strcmp(method, "numreceivers") == 0) {
        int numReceivers = atom_getint(argv + 1);
        ambi_roomsim_setNumReceivers(x->hAmbi, numReceivers);
    } else if (strcmp(method, "enableims") == 0) {
        // IMS significa Image Source Method, um método utilizado para simular reflexões precoces no
        // som.
        int enableIMS = atom_getint(argv + 1);
    } else if (strcmp(method, "maxreflectionorder") == 0) {
        int maxReflectionOrder = atom_getint(argv + 1);
        pd_assert(x, maxReflectionOrder < 7,
                  "[saf.roomsim~] Numbers higher then 7 is a very high reflection order");
        ambi_roomsim_setMaxReflectionOrder(x->hAmbi, maxReflectionOrder);
    } else if (strcmp(method, "wallabscoeff") == 0) {
        // set ambi_roomsim_setWallAbsCoeff
        // set wallabscoeff <+x> <-x> <+y> <-y> <+z> <-z>
        float coeffx_plus = atom_getfloat(argv + 1);
        float coeffx_minus = atom_getfloat(argv + 2);
        float coeffy_plus = atom_getfloat(argv + 3);
        float coeffy_minus = atom_getfloat(argv + 4);
        float coeffz_plus = atom_getfloat(argv + 5);
        float coeffz_minus = atom_getfloat(argv + 6);

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

    } else if (strcmp(method, "chorder") == 0) {
        int chOrder = atom_getint(argv + 1);
        ambi_roomsim_setChOrder(x->hAmbi, chOrder);
    } else if (strcmp(method, "normtype") == 0) {
        int normType = atom_getint(argv + 1);
        ambi_roomsim_setNormType(x->hAmbi, normType);
    } else {
        pd_error(x, "[saf.roomsim~] Unknown set method: %s", method);
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

        // Process only if a full frame is ready
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_roomsim_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            ambi_roomsim_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
    // This is a mess! Help is you see a better way.

    // ambi_roomsim_getFrameSize has fixed frameSize, for encoder is 64 for
    // decoder is 128. In the perform method sometimes I need to accumulate samples sometimes I
    // need to process 2 or more times to avoid change how ambi_roomsim_ works. I think that in this
    // way is more safe, once that these functions are tested in the main repo. But maybe worse
    // to implement the own set of functions.

    // Set frame sizes and reset indices
    x->nAmbiFrameSize = ambi_roomsim_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;
    int sum = x->nIn + x->nOut;
    int sigvecsize = sum + 2;

    // Initialize the ambisonic encoder
    if (!x->hAmbiInit) {
        ambi_roomsim_init(x->hAmbi, sys_getsr());
        ambi_roomsim_setOutputOrder(x->hAmbi, (SH_ORDERS)x->nOrder);
        ambi_roomsim_setNumSources(x->hAmbi, x->nIn);
        if (ambi_roomsim_getNSHrequired(x->hAmbi) < x->nOut) {
            pd_error(x, "[saf.encoder~] Number of output signals is too low for the %d order.",
                     x->nOrder);
            return;
        }
        x->hAmbiInit = 1;
    }

    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        ambi_roomsim_setNumSources(x->hAmbi, x->nIn);
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        ambiroom_tilde_malloc(x);
    }

    // add perform method
    if (x->multichannel) {
        x->nIn = sp[0]->s_nchans;
        ambi_roomsim_setNumSources(x->hAmbi, x->nIn);
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
    t_ambi_roomsim_tilde *x = (t_ambi_roomsim_tilde *)pd_new(ambiroom_tilde_class);
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

    ambi_roomsim_create(&x->hAmbi);
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
void ambiroom_tilde_free(t_ambi_roomsim_tilde *x) {
    printf("ambiroom_tilde_free\n");
    ambi_roomsim_destroy(&x->hAmbi);
    if (x->multichannel) {
        // TODO:
    } else {
        if (x->aIns) {
            for (int i = 0; i < x->nIn; i++) {
                freebytes(x->aIns[i], x->nAmbiFrameSize * sizeof(t_sample));
                freebytes(x->aInsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
            freebytes(x->aIns, x->nIn * sizeof(t_sample *));
            freebytes(x->aInsTmp, x->nIn * sizeof(t_sample *));
        }

        if (x->aOuts) {
            for (int i = 0; i < x->nOut; i++) {
                freebytes(x->aOuts[i], x->nAmbiFrameSize * sizeof(t_sample));
                freebytes(x->aOutsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
            freebytes(x->aOuts, x->nOut * sizeof(t_sample *));
            freebytes(x->aOutsTmp, x->nOut * sizeof(t_sample *));
        }
    }
}

// ─────────────────────────────────────
void setup_saf0x2eambiroom_tilde(void) {
    ambiroom_tilde_class = class_new(gensym("saf.encoder~"), (t_newmethod)ambiroom_tilde_new,
                                     (t_method)ambiroom_tilde_free, sizeof(t_ambi_roomsim_tilde),
                                     CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    logpost(NULL, 3, "[saf] is a pd version of Spatial Audio Framework by Leo McCormack");
    logpost(NULL, 3, "[saf] pd-saf by Charles K. Neimog");

    CLASS_MAINSIGNALIN(ambiroom_tilde_class, t_ambi_roomsim_tilde, sample);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(ambiroom_tilde_class, (t_method)ambiroom_tilde_set, gensym("set"), A_GIMME, 0);
}
