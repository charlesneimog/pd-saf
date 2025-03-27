#include <m_pd.h>
#include <pthread.h>
#include <string.h>

#include <ambi_roomsim.h>

static t_class *roomsim_tilde_class;

// ─────────────────────────────────────
typedef struct _roomsim_tilde {
    t_object obj;
    t_canvas *glist;
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
} t_roomsim_tilde;

// ╭─────────────────────────────────────╮
// │               Methods               │
// ╰─────────────────────────────────────╯
static void roomsim_tilde_set(t_roomsim_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;

    if (strcmp(method, "enableims") == 0) {
        int enableIMS = atom_getint(argv + 1);
        ambi_roomsim_setEnableIMSflag(x->hAmbi, enableIMS);
    } else if (strcmp(method, "maxreflectionorder") == 0) {
        int maxReflectionOrder = atom_getint(argv + 1);
        ambi_roomsim_setMaxReflectionOrder(x->hAmbi, maxReflectionOrder);
    } else if (strcmp(method, "sourcex") == 0) {
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
    } else if (strcmp(method, "numreceivers") == 0) {
        int numReceivers = atom_getint(argv + 1);
        ambi_roomsim_setNumReceivers(x->hAmbi, numReceivers);
    } else if (strcmp(method, "receiverx") == 0) {
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
    } else if (strcmp(method, "roomdimx") == 0) {
        float roomDimX = atom_getfloat(argv + 1) - 1;
        ambi_roomsim_setRoomDimX(x->hAmbi, roomDimX);
    } else if (strcmp(method, "roomdimy") == 0) {
        float roomDimY = atom_getfloat(argv + 1) - 1;
        ambi_roomsim_setRoomDimY(x->hAmbi, roomDimY);
    } else if (strcmp(method, "roomdimz") == 0) {
        float roomDimZ = atom_getfloat(argv + 1) - 1;
        ambi_roomsim_setRoomDimZ(x->hAmbi, roomDimZ);
    } else if (strcmp(method, "wallabscoeff") == 0) {
        int xyz_idx = atom_getint(argv + 1);
        int posNeg_idx = atom_getint(argv + 2);
        float absCoeff = atom_getfloat(argv + 3);
        ambi_roomsim_setWallAbsCoeff(x->hAmbi, xyz_idx, posNeg_idx, absCoeff);
    } else if (strcmp(method, "chorder") == 0) {
        int chOrder = atom_getint(argv + 1);
        ambi_roomsim_setChOrder(x->hAmbi, chOrder);
    } else if (strcmp(method, "normtype") == 0) {
        int normType = atom_getint(argv + 1);
        ambi_roomsim_setNormType(x->hAmbi, normType);
    } else {
        pd_error(x->glist, "[saf.roomsim~] Unknown set method: %s", method);
        return;
    }
}

// ╭─────────────────────────────────────╮
// │     Initialization and Perform      │
// ╰─────────────────────────────────────╯
// ─────────────────────────────────────
t_int *roomsim_tilde_perform(t_int *w) {
    t_roomsim_tilde *x = (t_roomsim_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n <= x->ambiFrameSize) {
        for (int ch = 0; ch < x->num_sources; ch++) {
            memcpy(x->ins[ch] + x->accumSize, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->accumSize += n;
        // Process only when we have a full ambisonic frame
        if (x->accumSize == x->ambiFrameSize) {
            ambi_roomsim_process(x->hAmbi, (const float *const *)x->ins, (float **)x->outs, x->num_sources,
                                 x->nSH, x->ambiFrameSize);
            x->accumSize = 0;
            x->outputIndex = 0;
        }
        // Output the processed samples in blocks
        for (int ch = 0; ch < x->nSH; ch++) {
            t_sample *out = (t_sample *)(w[3 + x->num_sources + ch]);
            memcpy(out, x->outs[ch] + x->outputIndex, n * sizeof(t_sample));
        }
        x->outputIndex += n;
    } else {
        // When n is greater than ambiFrameSize (e.g., frameSize=64 and n=128)
        int chunks = n / x->ambiFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            // Process each full chunk separately
            for (int ch = 0; ch < x->num_sources; ch++) {
                memcpy(x->ins_tmp[ch], (t_sample *)w[3 + ch] + (chunkIndex * x->ambiFrameSize),
                       x->ambiFrameSize * sizeof(t_sample));
            }
            ambi_roomsim_process(x->hAmbi, (const float *const *)x->ins_tmp, x->outs_tmp,
                                 x->num_sources, x->nSH, x->ambiFrameSize);
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
void roomsim_tilde_dsp(t_roomsim_tilde *x, t_signal **sp) {
    // This is a mess. ambi_enc_getFrameSize has fixed frameSize, for encoder is 64 for
    // decoder is 128. In the perform method somethimes I need to accumulate samples sometimes I
    // need to process 2 or more times to avoid change how ambi_enc_ works. I think that in this
    // way is more safe, once that this functions are tested in the main repo. But maybe worse
    // to implement the own set of functions.

    // Set frame sizes and reset indices
    x->ambiFrameSize = ambi_roomsim_getFrameSize();
    x->pdFrameSize = sp[0]->s_n;
    x->outputIndex = 0;
    x->accumSize = 0;
    int sum = x->num_sources + x->nSH;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    // add this in another thread
    // Initialize the ambisonic encoder
    if (!x->ambi_init) {
        ambi_roomsim_init(x->hAmbi, sys_getsr());
        ambi_roomsim_setOutputOrder(x->hAmbi, (SH_ORDERS)x->order);
        ambi_roomsim_setNumSources(x->hAmbi, x->num_sources);
        if (ambi_roomsim_getNSHrequired(x->hAmbi) < x->nSH) {
            pd_error(x, "[saf.encoder~] Number of output signals is too low for the %d order.",
                     x->order);
            return;
        }
        x->ambi_init = 1;
    }

    // Setup multi-out signals
    for (int i = x->num_sources; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    for (int i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }

    // Allocate arrays for input and output
    x->ins = (t_sample **)getbytes(x->num_sources * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    // IMPORTANT: Allocate ins_tmp based on num_sources (not nSH) to match processing below.
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

    dsp_addv(roomsim_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void *roomsim_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_roomsim_tilde *x = (t_roomsim_tilde *)pd_new(roomsim_tilde_class);
    x->glist = canvas_getcurrent();
    int order = (argc >= 1) ? atom_getint(argv) : 1;
    x->num_sources = (argc >= 2) ? atom_getint(argv + 1) : 1;

    x->order = order < 0 ? 0 : order;
    x->nSH = (x->order + 1) * (x->order + 1);
    x->ambi_init = 0;

    ambi_roomsim_create(&x->hAmbi);
    ambi_roomsim_setOutputOrder(x->hAmbi, order);
    ambi_roomsim_setNumSources(x->hAmbi, x->num_sources);

    for (int i = 1; i < x->num_sources; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->nSH; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    return (void *)x;
}

// ─────────────────────────────────────
void roomsim_tilde_free(t_roomsim_tilde *x) {
    ambi_roomsim_destroy(&x->hAmbi);
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
void setup_saf0x2eroomsim_tilde(void) {
    roomsim_tilde_class = class_new(gensym("saf.roomsim~"), (t_newmethod)roomsim_tilde_new,
                                    (t_method)roomsim_tilde_free, sizeof(t_roomsim_tilde),
                                    CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(roomsim_tilde_class, t_roomsim_tilde, sample);
    class_addmethod(roomsim_tilde_class, (t_method)roomsim_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(roomsim_tilde_class, (t_method)roomsim_tilde_set, gensym("set"), A_GIMME, 0);
}
