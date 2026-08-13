#include <string.h>
#include <pthread.h>

#include <m_pd.h>
#include <g_canvas.h>
#include <s_stuff.h>

#include <ambi_dec.h>
#include "utilities.h"

static t_class *decoder_tilde_class;

// ─────────────────────────────────────
typedef struct _decoder_tilde {
    t_object obj;
    t_canvas *glist;
    t_sample sample;

    void *hAmbi;

    t_sample **aIns;
    t_sample **aOuts;
    t_sample **aInsTmp;
    t_sample **aOutsTmp;

    char sofa_file[MAXPDSTRING];
    int use_sofa;

    int nAmbiFrameSize;
    int nPdFrameSize;
    int nInAccIndex;
    int nOutAccIndex;
    int nFlagSpeakers;

    int nOrder;
    int nIn;
    int nOut;
    int nPreviousIn;
    int nPreviousOut;

    int multichannel;
    int binaural;
} t_decoder_tilde;

// ─────────────────────────────────────
static void decoder_tilde_malloc(t_decoder_tilde *x) {
    if (x->aIns) {
        for (int i = 0; i < x->nPreviousIn; i++) {
            if (x->aIns[i]) {
                freebytes(x->aIns[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
            if (x->aInsTmp[i]) {
                freebytes(x->aInsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aIns, x->nPreviousIn * sizeof(t_sample *));
        freebytes(x->aInsTmp, x->nPreviousIn * sizeof(t_sample *));
        x->aIns = NULL;
        x->aInsTmp = NULL;
    }
    if (x->aOuts) {
        for (int i = 0; i < x->nPreviousOut; i++) {
            if (x->aOuts[i]) {
                freebytes(x->aOuts[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
            if (x->aOutsTmp[i]) {
                freebytes(x->aOutsTmp[i], x->nAmbiFrameSize * sizeof(t_sample));
            }
        }
        freebytes(x->aOuts, x->nPreviousOut * sizeof(t_sample *));
        freebytes(x->aOutsTmp, x->nPreviousOut * sizeof(t_sample *));
        x->aOuts = NULL;
        x->aOutsTmp = NULL;
    }

    // now allocate with the CURRENT x->nIn / x->nOut
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
static void decoder_tilde_configure_default_speakers(t_decoder_tilde *x) {
    if (x->nOut <= 0) {
        return;
    }

    if (x->nOut == 1) {
        ambi_dec_setLoudspeakerAzi_deg(x->hAmbi, 0, 0.f);
        ambi_dec_setLoudspeakerElev_deg(x->hAmbi, 0, 0.f);
        return;
    }

    float azimuth_step = 360.f / (float)x->nOut;
    float elevation = 0.f;

    for (int i = 0; i < x->nOut; i++) {
        float az = azimuth_step * (float)i;
        while (az > 180.f) {
            az -= 360.f;
        }
        while (az < -180.f) {
            az += 360.f;
        }

        logpost(x, 3, "[saf.decoder~] Setting default speaker %d to azimuth: %f elevation: %f",
                i + 1, az, elevation);
        ambi_dec_setLoudspeakerAzi_deg(x->hAmbi, i, az);
        ambi_dec_setLoudspeakerElev_deg(x->hAmbi, i, elevation);
    }
}

// ─────────────────────────────────────
void *decoder_tilde_initcodec(void *x_void) {
    t_decoder_tilde *x = (t_decoder_tilde *)x_void;
    ambi_dec_initCodec(x->hAmbi);
    const char *text = "[saf.decoder~] Decoder codec initialized!";
    char *d = strdup(text);
    pd_queue_mess(&pd_maininstance, &x->obj.te_g.g_pd, (void *)d, init_logcallback);
    return NULL;
}

// ╭─────────────────────────────────────╮
// │               Methods               │
// ╰─────────────────────────────────────╯
static void decoder_tilde_get(t_decoder_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;
    if (strcmp(method, "speakers") == 0) {
        int speakers_size = ambi_dec_getNumLoudspeakers(x->hAmbi);
        logpost(x, 2, "[saf.decoder~] There are %d speakers in the array", speakers_size);
        for (int i = 0; i < speakers_size; i++) {
            int azi = ambi_dec_getLoudspeakerAzi_deg(x->hAmbi, i);
            int ele = ambi_dec_getLoudspeakerAzi_deg(x->hAmbi, i);
            logpost(x, 2, "  index: %02d | azi %+04d | ele %+04d", i + 1, azi, ele);
        }
    }
}

// ─────────────────────────────────────
static void decoder_tilde_set(t_decoder_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = s->s_name;
    if (strcmp(method, "sofafile") == 0) {
        // Set sofa file
        char path[MAXPDSTRING];
        char *bufptr;
        t_symbol *sofa_path = atom_getsymbol(argv);
        int fd = canvas_open(x->glist, sofa_path->s_name, "", path, &bufptr, MAXPDSTRING, 1);
        if (fd > 1) {
            char completpath[MAXPDSTRING];
            pd_snprintf(completpath, MAXPDSTRING, "%s/%s", path, sofa_path->s_name);
            logpost(x, 2, "[saf.decoder~] Opening %s", completpath);
            strcpy(x->sofa_file, completpath);
            ambi_dec_setSofaFilePath(x->hAmbi, completpath);
            ambi_dec_setUseDefaultHRIRsflag(x->hAmbi, 0);
            x->use_sofa = 1;
        } else {
            pd_error(x->glist, "[saf.decoder~] Could not open sofa file!");
            ambi_dec_setUseDefaultHRIRsflag(x->hAmbi, 1);
            x->use_sofa = 1;
        }
    } else if (strcmp(method, "use_default_hrirs") == 0) {
        t_float state = atom_getfloat(argv);
        ambi_dec_setUseDefaultHRIRsflag(x->hAmbi, state);
    } else if (strcmp(method, "binaural") == 0) {
        if (argc < 1 || argv[0].a_type != A_FLOAT) {
            pd_error(x, "[saf.decoder~] 'binaural' expects 0 or 1");
            return;
        }

        x->binaural = atom_getfloat(argv) != 0;
        if (x->binaural) {
            x->nOut = 2;
        } else {
            x->nOut = x->nFlagSpeakers;
        }
        ambi_dec_setBinauraliseLSflag(x->hAmbi, x->binaural);
    } else if (strcmp(method, "speaker") == 0) {
        // The `loudspeaker` method sets the azimuth and elevation of a specific loudspeaker,
        // ensuring accurate spatial rendering of Ambisonic sources.
        int index = atom_getint(argv) - 1;
        float azi = atom_getfloat(argv + 1);
        float elev = atom_getfloat(argv + 2);
        int loudspeakercount = ambi_dec_getNumLoudspeakers(x->hAmbi);
        if (index < 0) {
            pd_error(x, "[saf.decoder~] %d is not a valid speaker index.", index + 1);
            return;
        } else if (loudspeakercount >= index - 1) {
            ambi_dec_setLoudspeakerAzi_deg(x->hAmbi, index, azi);
            ambi_dec_setLoudspeakerElev_deg(x->hAmbi, index, elev);
            logpost(x, 3, "[saf.decoder~] Setting loudspeaker position %d to %f %f", index + 1, azi,
                    elev);

            int lowfreq = ambi_dec_getDecMethod(x->hAmbi, 0);
            int highfreq = ambi_dec_getDecMethod(x->hAmbi, 1);
            if (lowfreq != DECODING_METHOD_ALLRAD) {
                ambi_dec_setDecMethod(x->hAmbi, 0, DECODING_METHOD_ALLRAD);
            }

            if (highfreq != DECODING_METHOD_ALLRAD) {
                ambi_dec_setDecMethod(x->hAmbi, 1, DECODING_METHOD_ALLRAD);
            }

        } else {
            pd_error(x,
                     "[saf.decoder~] Trying to set loudspeaker position %d, but only %d available.",
                     (int)index + 1, (int)loudspeakercount);
            return;
        }
    } else if (strcmp(method, "hrirpreproc") == 0) {
        // Enabling `hrirpreproc` applies pre-processing to the loaded HRTFs, improving consistency,
        // phase alignment, and spatial stability during binaural rendering.
        int state = atom_getint(argv);
        ambi_dec_setEnableHRIRsPreProc(x->hAmbi, state);
    } else if (strcmp(method, "ch_order") == 0) {
        // The `chorder` method sets the Ambisonic channel ordering convention (ACN or
        // Furse-Malham), determining how the Ambisonic channels are arranged in the signal.
        int order = atom_getint(argv);
        ambi_dec_setChOrder(x->hAmbi, order);
    } else if (strcmp(method, "normtype") == 0) {
        // The `normtype` method sets the Ambisonic normalization convention (N3D, SN3D, or
        // Furse-Malham), which defines how the channel amplitudes are scaled and affects the
        // overall energy and reconstruction of the sound field.
        int type = atom_getint(argv);
        ambi_dec_setNormType(x->hAmbi, type);

        if (type == NORM_N3D) {
            logpost(x, 2, "[saf.decoder~] Using orthonormalised (N3D)");
        } else if (type == NORM_SN3D) {
            logpost(x, 2, "[saf.decoder~] Schmidt semi-normalisation (SN3D)");
        } else if (type == NORM_FUMA) {
            logpost(x, 2, "[saf.decoder~] (Legacy) Furse-Malham scaling");
        } else {
            pd_error(x, "[saf.decoder~], Invalid normalization type");
        }

    } else if (strcmp(method, "decmethod") == 0) {
        // The `decmethod` method sets the Ambisonic decoding algorithm for either the low- or
        // high-frequency band, allowing selection between SAD, MMD, EPAD, or AllRAD. The decoders
        // are reinitialized in a separate thread to apply the change without interrupting audio
        // processing.
        // Here’s a concise one-sentence description for each decoding method:

        // * **SAD (Sampling Ambisonic Decoder):** Distributes sound energy to loudspeakers based on
        // their positions for a simple spatial rendering.
        // * **MMD (Mode-Matching Decoder):** Matches spherical harmonic modes to the speaker layout
        // for more accurate localization.
        // * **EPAD (Energy-Preserving Ambisonic Decoder):** Maintains the total energy of the sound
        // field, reducing amplitude artifacts.
        // * **AllRAD (All-Round Ambisonic Decoder):** Optimizes both directional accuracy and
        // energy preservation across all listening positions.

        t_symbol *low_high = atom_getsymbol(argv);
        int id = atom_getint(argv + 1);
        if (strcmp(low_high->s_name, "low") == 0) {
            ambi_dec_setDecMethod(x->hAmbi, 0, id);
        } else {
            ambi_dec_setDecMethod(x->hAmbi, 1, id);
        }
    } else if (strcmp(method, "max-rE") == 0) {
        // Max-rE weighting improves spatial accuracy and stability, preventing phantom source
        // splitting and reducing timbral coloration, regardless of listener position or Ambisonic
        // order. Check 'How to make Ambisonics sound good' from Matthias Frank.
        int index = atom_getint(argv + 1);
        int id = atom_getint(argv + 2);
        ambi_dec_setDecEnableMaxrE(x->hAmbi, index, id);
    } else if (strcmp(method, "transitionfreq") == 0) {
        // The `transitionfreq` method sets the crossover frequency between two Ambisonic decoders,
        // ensuring a smooth transition and consistent spatial rendering across low and high
        // frequencies.
        float freq = atom_getfloat(argv + 1);
        ambi_dec_setTransitionFreq(x->hAmbi, freq);
    } else if (strcmp(method, "numspeakers") == 0) {
        if (argc < 1 || argv[0].a_type != A_FLOAT) {
            pd_error(x, "[saf.decoder~] 'numspeakers' expects a speaker count");
            return;
        }

        int speakers = atom_getint(argv);
        int max_speakers = ambi_dec_getMaxNumLoudspeakers();
        if (speakers < 1 || speakers > max_speakers) {
            pd_error(x, "[saf.decoder~] Speaker count must be between 1 and %d", max_speakers);
            return;
        }
        if (!x->multichannel && speakers != x->nFlagSpeakers) {
            pd_error(x,
                     "[saf.decoder~] Cannot change the number of outlets after creation; use the "
                     "'-m' flag for a variable speaker count");
            return;
        }

        x->nFlagSpeakers = speakers;
        if (!x->binaural) {
            x->nOut = speakers;
        }
        ambi_dec_setNumLoudspeakers(x->hAmbi, speakers);
    } else {
        pd_error(x, "[saf.decoder~] '%s' not a valid method", method);
    }
    if (ambi_dec_getCodecStatus(x->hAmbi) == CODEC_STATUS_NOT_INITIALISED) {
        canvas_update_dsp();
    }
}

// ╭─────────────────────────────────────╮
// │     Initialization and Perform      │
// ╰─────────────────────────────────────╯
t_int *decoder_tilde_performmultichannel(t_int *w) {
    t_decoder_tilde *x = (t_decoder_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_sample *ins = (t_sample *)(w[3]);
    t_sample *outs = (t_sample *)(w[4]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, ins + (n * ch), n * sizeof(t_sample));
        }
        x->nInAccIndex += n;

        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_dec_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            ambi_dec_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
t_int *decoder_tilde_perform(t_int *w) {
    t_decoder_tilde *x = (t_decoder_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n < x->nAmbiFrameSize) {
        for (int ch = 0; ch < x->nIn; ch++) {
            memcpy(x->aIns[ch] + x->nInAccIndex, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->nInAccIndex += n;
        if (x->nInAccIndex == x->nAmbiFrameSize) {
            ambi_dec_process(x->hAmbi, (const float *const *)x->aIns, (float *const *)x->aOuts,
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
            ambi_dec_process(x->hAmbi, (const float *const *)x->aInsTmp,
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
void decoder_tilde_dsp(t_decoder_tilde *x, t_signal **sp) {
    if (!x->multichannel && sp[0]->s_nchans > 1) {
        for (int i = 0; i < x->nIn; i++) {
            signal_setmultiout(&sp[x->nIn], 1);
            dsp_add_zero(sp[i]->s_vec, sp[i]->s_n);
        }
        for (int i = 0; i < x->nOut; i++) {
            signal_setmultiout(&sp[x->nIn + i], 1);
            dsp_add_zero(sp[x->nIn + i]->s_vec, sp[x->nIn + i]->s_n);
        }
        pd_error(x, "[saf.decoder~] Expected mono input. Use -m flag to multichannel");
        return;
    }
    x->nAmbiFrameSize = ambi_dec_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;
    x->nIn = x->multichannel ? sp[0]->s_nchans : x->nIn;

    if (sp[0]->s_nchans != x->nIn && x->multichannel) {
        pd_error(x,
                 "Input signal has %d channels, but decoder is configured for %d channels. Update"
                 "the ambisonics order!",
                 sp[0]->s_nchans, x->nIn);
        return;
    }

    int nOrder = get_ambisonic_order(x->nOut);
    if (ambi_dec_getCodecStatus(x->hAmbi) == CODEC_STATUS_NOT_INITIALISED) {
        ambi_dec_setNormType(x->hAmbi, NORM_N3D);
        if (x->nOrder < 1 || x->binaural) {
            ambi_dec_setMasterDecOrder(x->hAmbi, 1);
            ambi_dec_setBinauraliseLSflag(x->hAmbi, 1);
        } else {
            ambi_dec_setMasterDecOrder(x->hAmbi, nOrder);
            ambi_dec_setBinauraliseLSflag(x->hAmbi, 0);
            int preset = get_loudspeaker_array_preset(x->nOut);
            if (preset == -1) {
                pd_error(
                    x,
                    "Unknown preset for loudspeakers: %d. Please set using the 'speaker' message",
                    x->nOut);
            }
            ambi_dec_setOutputConfigPreset(x->hAmbi, preset);
        }

        ambi_dec_setNumLoudspeakers(x->hAmbi, x->nFlagSpeakers);

        // decoder_tilde_configure_default_speakers(x);

        ambi_dec_setDecMethod(x->hAmbi, DECODING_METHOD_SAD, 0);
        ambi_dec_setDecMethod(x->hAmbi, DECODING_METHOD_SAD, 1);

        logpost(x, 2, "[saf.decoder~] Initializing decoder codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, decoder_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        decoder_tilde_malloc(x);
        x->nPreviousIn = x->nIn;
        x->nPreviousOut = x->nOut;
    }

    // Initialize memory allocation for inputs and outputs
    if (x->multichannel) {
        if (x->binaural) {
            x->nOut = 2;
        } else {
            x->nOut = x->nFlagSpeakers;
        }
        signal_setmultiout(&sp[1], x->nOut);
        dsp_add(decoder_tilde_performmultichannel, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
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
        dsp_addv(decoder_tilde_perform, sigvecsize, sigvec);
        freebytes(sigvec, sigvecsize * sizeof(t_int));
    }
}

// ─────────────────────────────────────
void *decoder_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_decoder_tilde *x = (t_decoder_tilde *)pd_new(decoder_tilde_class);
    x->glist = canvas_getcurrent(); // TODO: add HRIR reader

    int order = 1;
    int num_loudspeakers = 4;
    if (argc == 0) {
        x->multichannel = 0;
    } else {
        if (argv[0].a_type == A_SYMBOL) {
            if (strcmp(atom_getsymbol(argv)->s_name, "-m") != 0) {
                pd_error(x, "[saf.decoder~] Expected '-m' in second argument.");
                return NULL;
            }
            order = 1;
            num_loudspeakers = (argc >= 2) ? atom_getint(argv + 1) : 4;
            x->multichannel = 1;
        } else {
            order = (argc >= 1) ? atom_getint(argv) : 1;
            num_loudspeakers = (argc >= 2) ? atom_getint(argv + 1) : 1;
            x->multichannel = 0;
            post("decoder mono");
        }
    }

    order = order < 0 ? 0 : order;
    num_loudspeakers = num_loudspeakers < 1 ? 1 : num_loudspeakers;

    x->nOrder = get_ambisonic_order(num_loudspeakers);
    x->nIn = (order + 1) * (order + 1);
    x->nOut = num_loudspeakers;
    x->nFlagSpeakers = x->nOut;
    ambi_dec_create(&x->hAmbi);
    ambi_dec_setNumLoudspeakers(x->hAmbi, x->nOut);
    ambi_dec_init(x->hAmbi, sys_getsr());

    if (x->nOrder < 1) {
        x->nOut = 2;
        x->binaural = 1;
    }

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
void decoder_tilde_free(t_decoder_tilde *x) {
    ambi_dec_destroy(&x->hAmbi);
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
// clang-format off
void setup_saf0x2edecoder_tilde(void) {
    decoder_tilde_class = class_new(gensym("saf.decoder~"), (t_newmethod)decoder_tilde_new,
                                    (t_method)decoder_tilde_free, sizeof(t_decoder_tilde),
                                    CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(decoder_tilde_class, t_decoder_tilde, sample);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_dsp, gensym("dsp"), A_CANT, 0);

    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("sofafile"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("binaural"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("decoder_order"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("speaker"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("hrirpreproc"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("ch_order"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("normtype"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("decmethod"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("max-rE"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("transitionfreq"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("numspeakers"), A_GIMME, 0);
}
