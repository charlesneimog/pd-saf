#include <string.h>
#include <math.h>
#include <pthread.h>

#include <m_pd.h>
#include <g_canvas.h>
#include <s_stuff.h>

#include <ambi_bin.h>

static t_class *binaural_tilde_class;

// ─────────────────────────────────────
typedef struct _binaural_tilde {
    t_object obj;
    t_canvas *glist;
    t_sample sample;

    void *hAmbi;

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
} t_binaural_tilde;

// ─────────────────────────────────────
void *binaural_tilde_initcodec(void *x_void) {
    t_binaural_tilde *x = (t_binaural_tilde *)x_void;
    ambi_bin_initCodec(x->hAmbi);
    logpost(x, 2, "[saf.binaural~] binaural codec initialized!");
    return NULL;
}

// ─────────────────────────────────────
static void binaural_tilde_malloc(t_binaural_tilde *x) {
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

// ╭─────────────────────────────────────╮
// │               Methods               │
// ╰─────────────────────────────────────╯
static void binaural_tilde_set(t_binaural_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;
    if (strcmp(method, "sofafile") == 0) {
        char path[MAXPDSTRING];
        char *bufptr;
        t_symbol *sofa_path = atom_getsymbol(argv + 1);
        int fd = canvas_open(x->glist, sofa_path->s_name, "", path, &bufptr, MAXPDSTRING, 1);
        if (fd > 1) {
            char completpath[MAXPDSTRING];
            pd_snprintf(completpath, MAXPDSTRING, "%s/%s", path, sofa_path->s_name);
            logpost(x, 2, "[saf.binaural~] Opening %s", completpath);
            ambi_bin_setSofaFilePath(x->hAmbi, completpath);
        } else {
            pd_error(x->glist, "[saf.binaural~] Could not open sofa file!");
        }
    } else if (strcmp(method, "usedefaulthrirs") == 0) {
        t_float useDefault = atom_getfloat(argv + 1);
        ambi_bin_setUseDefaultHRIRsflag(x->hAmbi, useDefault);
    } else if (strcmp(method, "inputorderpreset") == 0) {
        SH_ORDERS orderPreset = (SH_ORDERS)atom_getint(argv + 1);
        ambi_bin_setInputOrderPreset(x->hAmbi, orderPreset);
    } else if (strcmp(method, "decodingmethod") == 0) {
        AMBI_BIN_DECODING_METHODS decodingMethod = (AMBI_BIN_DECODING_METHODS)atom_getint(argv + 1);
        if (decodingMethod > 4 && decodingMethod < 1) {
            logpost(x, 2, "[saf.binaural~] Invalid decoding method!");
            logpost(x, 2, "    1: Least-squares (LS) decoder");
            logpost(x, 2,
                    "    2: least-squares (ls) decoder with diffuse-field spectral equalisation");
            logpost(x, 2, "    3: Time-alignment (TA) (default)");
            logpost(x, 2, "    4: Magnitude least-squares decoder (MagLS)");
        }

        ambi_bin_setDecodingMethod(x->hAmbi, decodingMethod);
    } else if (strcmp(method, "chorder") == 0) {
        int chOrder = atom_getint(argv + 1);
        // HOA_CH_ORDER_ACN,  /**< Ambisonic Channel numbering (ACN) convention, which
        //                     *   is employed by all spherical harmonic related
        //                     *   functions in SAF */
        // HOA_CH_ORDER_FUMA  /**< Furse-Malham (FuMa) convention, often used by older
        //                     *   recordings. The convention follows the WXYZ ordering
        //                     *   of the omni and dipoles, and is suitable only for
        //                     *   1st order. */

        ambi_bin_setChOrder(x->hAmbi, chOrder);
    } else if (strcmp(method, "normtype") == 0) {
        /**
         * Available Ambisonic normalisation conventions
         *
         * @warning NORM_FUMA is only supported for first order input! It also  has the
         *          1/sqrt(2) scaling term applied to the omni.
         */
        // typedef enum {
        //     NORM_N3D = 1, /**< orthonormalised (N3D) */
        //     NORM_SN3D,    /**< Schmidt semi-normalisation (SN3D) */
        //     NORM_FUMA     /**< (Legacy) Furse-Malham scaling */
        //
        // } NORM_TYPES;

        int normType = atom_getint(argv + 1);
        ambi_bin_setNormType(x->hAmbi, normType);
    } else if (strcmp(method, "enablemaxre") == 0) {
        int enableMaxRE = atom_getint(argv + 1);
        ambi_bin_setEnableMaxRE(x->hAmbi, enableMaxRE);
    } else if (strcmp(method, "enablediffusematching") == 0) {
        int enableDiffuse = atom_getint(argv + 1);
        ambi_bin_setEnableDiffuseMatching(x->hAmbi, enableDiffuse);
    } else if (strcmp(method, "enabletruncationeq") == 0) {
        int enableTruncation = atom_getint(argv + 1);
        ambi_bin_setEnableTruncationEQ(x->hAmbi, enableTruncation);
    } else if (strcmp(method, "hrirspreproc") == 0) {
        AMBI_BIN_PREPROC preProcType = (AMBI_BIN_PREPROC)atom_getint(argv + 1);
        ambi_bin_setHRIRsPreProc(x->hAmbi, preProcType);
    } else if (strcmp(method, "enablerotation") == 0) {
        int enableRotation = atom_getint(argv + 1);
        ambi_bin_setEnableRotation(x->hAmbi, enableRotation);
    } else if (strcmp(method, "yaw") == 0) {
        float yaw = atom_getfloat(argv + 1);
        ambi_bin_setYaw(x->hAmbi, yaw);
    } else if (strcmp(method, "pitch") == 0) {
        float pitch = atom_getfloat(argv + 1);
        ambi_bin_setPitch(x->hAmbi, pitch);
    } else if (strcmp(method, "roll") == 0) {
        float roll = atom_getfloat(argv + 1);
        ambi_bin_setRoll(x->hAmbi, roll);
    } else if (strcmp(method, "flipyaw") == 0) {
        int flipYaw = atom_getint(argv + 1);
        ambi_bin_setFlipYaw(x->hAmbi, flipYaw);
    } else if (strcmp(method, "flippitch") == 0) {
        int flipPitch = atom_getint(argv + 1);
        ambi_bin_setFlipPitch(x->hAmbi, flipPitch);
    } else if (strcmp(method, "fliproll") == 0) {
        int flipRoll = atom_getint(argv + 1);
        ambi_bin_setFlipRoll(x->hAmbi, flipRoll);
    } else if (strcmp(method, "rpyflag") == 0) {
        int rpyFlag = atom_getint(argv + 1);
        ambi_bin_setRPYflag(x->hAmbi, rpyFlag);
    } else {
        pd_error(x, "[saf.binaural~] Unknown set method: %s", method);
    }
}

// ─────────────────────────────────────
t_int *binaural_tilde_performmultichannel(t_int *w) {
    t_binaural_tilde *x = (t_binaural_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, ins + (n * ch), n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_bin_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            // Copia os dados de entrada para cada canal
            for (int ch = 0; ch < x->nIn; ch++) {
                memcpy(x->aInsTmp[ch], (t_sample *)w[3] + ch * n + chunkIndex * x->nAmbiFrameSize,
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
            // Processa o bloco atual
            ambi_bin_process(x->hAmbi, (const float *const *)x->aInsTmp,
                             (float *const *)x->aOutsTmp, x->nIn, x->nOut, x->nAmbiFrameSize);

            t_sample *out = (t_sample *)(w[4]);
            for (int ch = 0; ch < x->nOut; ch++) {
                memcpy(out + ch * n + chunkIndex * x->nAmbiFrameSize, x->aOutsTmp[ch],
                       x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 5);
}

// ─────────────────────────────────────
t_int *binaural_tilde_perform(t_int *w) {
    t_binaural_tilde *x = (t_binaural_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_bin_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            ambi_bin_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
void binaural_tilde_dsp(t_binaural_tilde *x, t_signal **sp) {
    // Set frame sizes and reset indices
    x->nAmbiFrameSize = ambi_bin_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;

    // add this in another thread
    if (ambi_bin_getProgressBar0_1(x->hAmbi) != 1 && ambi_bin_getProgressBar0_1(x->hAmbi) == 0) {
        ambi_bin_setYaw(x->hAmbi, 180.0f);
        ambi_bin_init(x->hAmbi, sys_getsr());
        logpost(x, 2, "[saf.decoder~] initializing decoder codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, binaural_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        binaural_tilde_malloc(x);
    }

    // Initialize memory allocation for inputs and outputs
    if (x->multichannel) {
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(binaural_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
    } else {
        int sum = x->nIn + x->nOut;
        int sigvecsize = sum + 2;
        for (int i = x->nIn; i < sum; i++) {
            signal_setmultiout(&sp[i], 1);
        }
        t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));
        sigvec[0] = (t_int)x;
        sigvec[1] = (t_int)sp[0]->s_n;
        for (int i = 0; i < sum; i++) {
            sigvec[2 + i] = (t_int)sp[i]->s_vec;
        }
        dsp_addv(binaural_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *binaural_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_binaural_tilde *x = (t_binaural_tilde *)pd_new(binaural_tilde_class);
    x->glist = canvas_getcurrent(); // TODO: add HRIR reader
    int order = (argc >= 1) ? atom_getint(argv) : 1;
    int num_loudspeakers = (argc >= 2) ? atom_getint(argv + 1) : 2;
    x->multichannel = (argc >= 3) ? strcmp(atom_getsymbol(argv + 2)->s_name, "-m") == 0 : 0;

    if (argc >= 1) {
        order = atom_getint(argv);
    }
    if (argc >= 2) {
        num_loudspeakers = atom_getint(argv + 1);
    }

    order = order < 0 ? 0 : order;
    num_loudspeakers = num_loudspeakers < 1 ? 1 : num_loudspeakers;

    x->nOrder = (int)floor(sqrt(num_loudspeakers) - 1);
    x->nIn = (order + 1) * (order + 1);
    x->nOut = num_loudspeakers;
    ambi_bin_create(&x->hAmbi);

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
    x->aIns = NULL;
    x->aOuts = NULL;

    return (void *)x;
}

// ─────────────────────────────────────
void binaural_tilde_free(t_binaural_tilde *x) {
    ambi_bin_destroy(&x->hAmbi);
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
void setup_saf0x2ebinaural_tilde(void) {
    binaural_tilde_class = class_new(gensym("saf.binaural~"), (t_newmethod)binaural_tilde_new,
                                     (t_method)binaural_tilde_free, sizeof(t_binaural_tilde),
                                     CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(binaural_tilde_class, t_binaural_tilde, sample);
    class_addmethod(binaural_tilde_class, (t_method)binaural_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(binaural_tilde_class, (t_method)binaural_tilde_set, gensym("set"), A_GIMME, 0);
}
