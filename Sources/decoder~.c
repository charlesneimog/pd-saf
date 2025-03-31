#include <string.h>
#include <math.h>
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
    int hAmbiInit;

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
} t_decoder_tilde;

// ─────────────────────────────────────
static void decoder_tilde_malloc(t_decoder_tilde *x) {
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
void *decoder_tilde_initcodec(void *x_void) {
    t_decoder_tilde *x = (t_decoder_tilde *)x_void;
    ambi_dec_initCodec(x->hAmbi);
    logpost(x, 2, "[saf.decoder~] decoder codec initialized!");
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
            ambi_dec_setSofaFilePath(x->hAmbi, completpath);
        } else {
            pd_error(x->glist, "[saf.decoder~] Could not open sofa file!");
        }
    } else if (strcmp(method, "binaural") == 0) {
        t_float binaural = atom_getfloat(argv + 1);
        ambi_dec_setBinauraliseLSflag(x->hAmbi, binaural);
        logpost(x, 2, "[saf.decoder~] reinit decoder codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, decoder_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
    } else if (strcmp(method, "defaultHRIR") == 0) {
        t_float defaultHRIR = atom_getfloat(argv + 1);
        ambi_dec_setUseDefaultHRIRsflag(x->hAmbi, defaultHRIR);
    } else if (strcmp(method, "masterDecOrder") == 0) {
        int order = atom_getint(argv + 1);
        ambi_dec_setMasterDecOrder(x->hAmbi, order);
    } else if (strcmp(method, "order") == 0) {
        int order = atom_getint(argv + 1);
        int bandIdx = atom_getint(argv + 2);
        ambi_dec_setDecOrder(x->hAmbi, order, bandIdx);
    } else if (strcmp(method, "orderallbands") == 0) {
        int order = atom_getint(argv + 1);
        ambi_dec_setDecOrderAllBands(x->hAmbi, order);
    } else if (strcmp(method, "loudspeakerpos") == 0) {
        int index = atom_getint(argv + 1) - 1;
        float azi = atom_getfloat(argv + 2);
        float elev = atom_getfloat(argv + 3);
        int loudspeakercount = ambi_dec_getNumLoudspeakers(x->hAmbi);
        if (index < 0) {
            pd_error(x, "[saf.decoder~] %d is not a valid speaker index.", index + 1);
            return;

        } else if (loudspeakercount >= index) {
            ambi_dec_setLoudspeakerAzi_deg(x->hAmbi, index, azi);
            ambi_dec_setLoudspeakerElev_deg(x->hAmbi, index, elev);
            logpost(x, 3, "[saf.decoder~] Setting loudspeaker position %d to %f %f", index + 1, azi,
                    elev);
            ambi_dec_refreshSettings(x->hAmbi);
        } else {
            pd_error(x,
                     "[saf.decoder~] Trying to set loudspeaker position %d, but only %d available.",
                     (int)index + 1, (int)loudspeakercount);
            return;
        }
    } else if (strcmp(method, "hrirpreproc") == 0) {
        int state = atom_getint(argv + 1);
        ambi_dec_setEnableHRIRsPreProc(x->hAmbi, state);
    } else if (strcmp(method, "sourcepreset") == 0) {
        int preset = atom_getint(argv + 1);
        ambi_dec_setSourcePreset(x->hAmbi, preset);
    } else if (strcmp(method, "outconfigpreset") == 0) {
        int preset = atom_getint(argv + 1);
        ambi_dec_setOutputConfigPreset(x->hAmbi, preset);
    } else if (strcmp(method, "chorder") == 0) {
        int order = atom_getint(argv + 1);
        ambi_dec_setChOrder(x->hAmbi, order);
    } else if (strcmp(method, "normtype") == 0) {
        int type = atom_getint(argv + 1);
        ambi_dec_setNormType(x->hAmbi, type);
    } else if (strcmp(method, "decMethod") == 0) {
        int index = atom_getint(argv + 1);
        int id = atom_getint(argv + 2);
        ambi_dec_setDecMethod(x->hAmbi, index, id);
    } else if (strcmp(method, "decenablemaxre") == 0) {
        int index = atom_getint(argv + 1);
        int id = atom_getint(argv + 2);
        ambi_dec_setDecEnableMaxrE(x->hAmbi, index, id);
    } else if (strcmp(method, "normtype") == 0) {
        int index = atom_getint(argv + 1);
        int id = atom_getint(argv + 2);
        ambi_dec_setDecNormType(x->hAmbi, index, id);
    } else if (strcmp(method, "transitionfreq") == 0) {
        float freq = atom_getfloat(argv + 1);
        ambi_dec_setTransitionFreq(x->hAmbi, freq);
    } else {
        pd_error(x, "[saf.decoder~] Unknown set method: %s", method);
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
    // Set frame sizes and reset indices
    x->nAmbiFrameSize = ambi_dec_getFrameSize();
    x->nPdFrameSize = sp[0]->s_n;
    x->nOutAccIndex = 0;
    x->nInAccIndex = 0;
    x->nIn = x->multichannel ? sp[0]->s_nchans : x->nIn;

    int nOrder = get_ambisonic_order(x->nOut);
    if (nOrder != x->nOrder || !x->hAmbiInit) {
        int preset = get_loudspeaker_array_preset(x->nOut);
        if (preset == LOUDSPEAKER_ARRAY_PRESET_DEFAULT) {
            logpost(x, 3, "[saf.decoder~] default loudspeaker preset is 4 speakers");
            preset = LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_4;
        }
        ambi_dec_setNumLoudspeakers(x->hAmbi, x->nOut);
        ambi_dec_setOutputConfigPreset(x->hAmbi, preset);
        ambi_dec_setMasterDecOrder(x->hAmbi, nOrder);
        ambi_dec_setUseDefaultHRIRsflag(x->hAmbi, 1);
        ambi_dec_init(x->hAmbi, sys_getsr());

        int required = ambi_dec_getNSHrequired(x->hAmbi);
        if (required > x->nOut) {
            pd_error(x, "[saf.decoder~] %d output signals is too low for the %d order.", required,
                     x->nOrder);
            return;
        }

        logpost(x, 2, "[saf.decoder~] initializing decoder codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, decoder_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
        x->hAmbiInit = 1;
    }
    x->nOrder = nOrder;

    // add this in another thread
    if (x->nOrder < 1) {
        ambi_dec_setBinauraliseLSflag(x->hAmbi, 1);
        logpost(x, 2, "[saf.decoder~] Order too low, enabling binauralisation...");
    }

    if (x->nPreviousIn != x->nIn || x->nPreviousOut != x->nOut) {
        decoder_tilde_malloc(x);
    }

    // Initialize memory allocation for inputs and outputs
    if (x->multichannel) {
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
    if (x->nOrder < 1) {
        pd_error(x, "[saf.decoder~] Minimal order is 1 (requires at least 4 loudspeakers). "
                    "Falling back to binaural mode.");
    }
    x->hAmbiInit = 0;
    ambi_dec_create(&x->hAmbi);

    int preset = get_loudspeaker_array_preset(x->nOut);
    ambi_dec_setNumLoudspeakers(x->hAmbi, x->nOut);
    ambi_dec_setOutputConfigPreset(x->hAmbi, preset);

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
void setup_saf0x2edecoder_tilde(void) {
    decoder_tilde_class = class_new(gensym("saf.decoder~"), (t_newmethod)decoder_tilde_new,
                                    (t_method)decoder_tilde_free, sizeof(t_decoder_tilde),
                                    CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(decoder_tilde_class, t_decoder_tilde, sample);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_set, gensym("set"), A_GIMME, 0);
    class_addmethod(decoder_tilde_class, (t_method)decoder_tilde_get, gensym("get"), A_GIMME, 0);
}
