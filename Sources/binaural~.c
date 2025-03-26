#include <pthread.h>
#include <string.h>

#include <ambi_bin.h>
#include <m_pd.h>

static t_class *binaural_tilde_class;

// ─────────────────────────────────────
typedef struct _binaural_tilde {
    t_object obj;
    t_canvas *glist;
    t_sample sample;

    void *hAmbi;

    t_sample **ins;
    t_sample **outs;
    t_sample **ins_tmp;
    t_sample **outs_tmp;

    int ambiFrameSize;
    int pdFrameSize;
    int accumSize;

    int order;
    int nSH;
    int num_loudspeakers;

    int outputIndex;
} t_binaural_tilde;

// ─────────────────────────────────────
void *binaural_tilde_initcodec(void *x_void) {
    t_binaural_tilde *x = (t_binaural_tilde *)x_void;
    ambi_bin_initCodec(x->hAmbi);
    logpost(x, 2, "[saf.binaural~] binaural codec initialized!");
    return NULL;
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
            snprintf(completpath, MAXPDSTRING, "%s/%s", path, sofa_path->s_name);
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
t_int *binaural_tilde_perform(t_int *w) {
    t_binaural_tilde *x = (t_binaural_tilde *)(w[1]);
    int n = (int)(w[2]);
    int init = ambi_bin_getProgressBar0_1(x->hAmbi);
    if (init != 1) {
        for (int ch = 0; ch < x->num_loudspeakers; ch++) {
            t_sample *out = (t_sample *)(w[3 + x->nSH + ch]);
            memset(out, 0, n * sizeof(t_sample));
        }
        return (w + 3 + x->nSH + x->num_loudspeakers);
    }

    if (n <= x->ambiFrameSize) {
        // Accumulate input samples
        for (int ch = 0; ch < x->nSH; ch++) {
            memcpy(x->ins[ch] + x->accumSize, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->accumSize += n;
        // Process only when we have a full ambisonic frame
        if (x->accumSize == x->ambiFrameSize) {
            ambi_bin_process(x->hAmbi, (const float *const *)x->ins, x->outs, x->nSH,
                             x->num_loudspeakers, x->ambiFrameSize);
            x->accumSize = 0;
            x->outputIndex = 0;
        }
        // Output the processed samples in blocks
        for (int ch = 0; ch < x->num_loudspeakers; ch++) {
            t_sample *out = (t_sample *)(w[3 + x->nSH + ch]);
            memcpy(out, x->outs[ch] + x->outputIndex, n * sizeof(t_sample));
        }
        x->outputIndex += n;
    } else {
        // When n is greater than ambiFrameSize (e.g., frameSize=64 and n=128)
        int chunks = n / x->ambiFrameSize;
        for (int chunkIndex = 0; chunkIndex < chunks; chunkIndex++) {
            // Process each full chunk separately
            for (int ch = 0; ch < x->nSH; ch++) {
                memcpy(x->ins_tmp[ch], (t_sample *)w[3 + ch] + (chunkIndex * x->ambiFrameSize),
                       x->ambiFrameSize * sizeof(t_sample));
            }
            ambi_bin_process(x->hAmbi, (const float *const *)x->ins_tmp, x->outs_tmp, x->nSH,
                             x->num_loudspeakers, x->ambiFrameSize);
            for (int ch = 0; ch < x->num_loudspeakers; ch++) {
                t_sample *out = (t_sample *)(w[3 + x->nSH + ch]);
                memcpy(out + (chunkIndex * x->ambiFrameSize), x->outs_tmp[ch],
                       x->ambiFrameSize * sizeof(t_sample));
            }
        }
    }

    return (w + 3 + x->nSH + x->num_loudspeakers);
}

// ─────────────────────────────────────
void binaural_tilde_dsp(t_binaural_tilde *x, t_signal **sp) {
    // This is a mess. ambi_enc_getFrameSize has fixed frameSize, for encoder is
    // 64 for decoder is 128. In the perform method somethimes I need to
    // accumulate samples sometimes I need to process 2 or more times to avoid
    // change how ambi_enc_ works. I think that in this way is more safe, once
    // that this functions are tested in the main repo. But maybe worse to
    // implement the own set of functions.

    // Set frame sizes and reset indices
    x->ambiFrameSize = ambi_bin_getFrameSize();
    x->pdFrameSize = sp[0]->s_n;
    x->outputIndex = 0;
    x->accumSize = 0;
    int sum = x->nSH + x->num_loudspeakers;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    // call another thread here
    if (ambi_bin_getProgressBar0_1(x->hAmbi) != 1 && ambi_bin_getProgressBar0_1(x->hAmbi) == 0) {

        ambi_bin_init(x->hAmbi, sys_getsr());
        logpost(x, 2, "[saf.binaural~] initializing binaural codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, binaural_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
    }

    // Setup multi-out signals
    for (int i = x->nSH; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    for (int i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }

    // Allocate arrays for input and output
    x->ins = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(x->num_loudspeakers * sizeof(t_sample *));
    // IMPORTANT: Allocate ins_tmp based on num_sources (not nSH) to match
    // processing below.
    x->ins_tmp = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    x->outs_tmp = (t_sample **)getbytes(x->num_loudspeakers * sizeof(t_sample *));

    for (int i = 0; i < x->nSH; i++) {
        x->ins[i] = (t_sample *)getbytes(x->ambiFrameSize * sizeof(t_sample));
        for (int j = 0; j < x->ambiFrameSize; j++) {
            x->ins[i][j] = 0;
        }
        x->ins_tmp[i] = (t_sample *)getbytes(sp[0]->s_n * sizeof(t_sample));
    }

    for (int i = 0; i < x->num_loudspeakers; i++) {
        x->outs[i] = (t_sample *)getbytes(x->ambiFrameSize * sizeof(t_sample));
        for (int j = 0; j < x->ambiFrameSize; j++) {
            x->outs[i][j] = 0;
        }
        x->outs_tmp[i] = (t_sample *)getbytes(sp[0]->s_n * sizeof(t_sample));
    }

    dsp_addv(binaural_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void *binaural_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_binaural_tilde *x = (t_binaural_tilde *)pd_new(binaural_tilde_class);
    x->glist = canvas_getcurrent();
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

    ambi_bin_create(&x->hAmbi);
    ambi_bin_setNormType(x->hAmbi, NORM_N3D);
    ambi_bin_setInputOrderPreset(x->hAmbi, (SH_ORDERS)order);
    ambi_bin_setUseDefaultHRIRsflag(x->hAmbi, 1);

    for (int i = 1; i < x->nSH; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->num_loudspeakers; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    return (void *)x;
}

// ─────────────────────────────────────
void binaural_tilde_free(t_binaural_tilde *x) {
    if (x->ins) {
        for (int i = 0; i < x->nSH; i++) {
            freebytes(x->ins[i], x->ambiFrameSize * sizeof(t_sample));
            freebytes(x->ins_tmp[i], x->pdFrameSize * sizeof(t_sample));
        }
        freebytes(x->ins, x->nSH * sizeof(t_sample *));
        freebytes(x->ins_tmp, x->nSH * sizeof(t_sample *));
    }

    if (x->outs) {
        for (int i = 0; i < x->num_loudspeakers; i++) {
            freebytes(x->outs[i], x->ambiFrameSize * sizeof(t_sample));
            freebytes(x->outs_tmp[i], x->pdFrameSize * sizeof(t_sample));
        }
        freebytes(x->outs, x->num_loudspeakers * sizeof(t_sample *));
        freebytes(x->outs_tmp, x->pdFrameSize * sizeof(t_sample *));
    }
    ambi_bin_destroy(&x->hAmbi);
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
