#include <string.h>
#include <math.h>
#include <pthread.h>

#include <m_pd.h>
#include <g_canvas.h>
#include <s_stuff.h>

#include <ambi_dec.h>

static t_class *decoder_tilde_class;

// ─────────────────────────────────────
typedef struct _decoder_tilde {
    t_object obj;
    t_canvas *glist;
    void *hAmbi;
    t_sample sample;

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
} t_decoder_tilde;

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
        pd_error(x->glist, "[saf.decoder~] Unknown set method: %s", method);
        return;
    }
}

// ╭─────────────────────────────────────╮
// │     Initialization and Perform      │
// ╰─────────────────────────────────────╯
t_int *decoder_tilde_perform(t_int *w) {
    t_decoder_tilde *x = (t_decoder_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n <= x->ambiFrameSize) {
        // Accumulate input samples
        for (int ch = 0; ch < x->nSH; ch++) {
            memcpy(x->ins[ch] + x->accumSize, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->accumSize += n;
        // Process only when we have a full ambisonic frame
        if (x->accumSize == x->ambiFrameSize) {
            ambi_dec_process(x->hAmbi, (const float *const *)x->ins, x->outs, x->nSH,
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
            ambi_dec_process(x->hAmbi, (const float *const *)x->ins_tmp, x->outs_tmp, x->nSH,
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
void decoder_tilde_dsp(t_decoder_tilde *x, t_signal **sp) {
    // This is a mess. ambi_enc_getFrameSize has fixed frameSize, for encoder is 64 for
    // decoder is 128. In the perform method somethimes I need to accumulate samples sometimes I
    // need to process 2 or more times to avoid change how ambi_enc_ works. I think that in this
    // way is more safe, once that this functions are tested in the main repo. But maybe worse
    // to implement the own set of functions.

    // Set frame sizes and reset indices
    x->ambiFrameSize = ambi_dec_getFrameSize();
    x->pdFrameSize = sp[0]->s_n;
    x->outputIndex = 0;
    x->accumSize = 0;
    int sum = x->nSH + x->num_loudspeakers;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    // add this in another thread

    // Setup multi-out signals
    for (int i = x->nSH; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    if (x->order < 1) {
        ambi_dec_setBinauraliseLSflag(x->hAmbi, 1);
    }

    if (ambi_dec_getProgressBar0_1(x->hAmbi) != 1 && ambi_dec_getProgressBar0_1(x->hAmbi) == 0) {
        // Initialize the ambisonic encoder
        ambi_dec_init(x->hAmbi, sys_getsr());
        logpost(x, 2, "[saf.decoder~] initializing decoder codec...");
        pthread_t initThread;
        pthread_create(&initThread, NULL, decoder_tilde_initcodec, (void *)x);
        pthread_detach(initThread);
    }

    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    for (int i = 0; i < sum; i++) {
        sigvec[2 + i] = (t_int)sp[i]->s_vec;
    }

    // Allocate arrays for input and output
    x->ins = (t_sample **)getbytes(x->nSH * sizeof(t_sample *));
    x->outs = (t_sample **)getbytes(x->num_loudspeakers * sizeof(t_sample *));
    // IMPORTANT: Allocate ins_tmp based on num_sources (not nSH) to match processing below.
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

    dsp_addv(decoder_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void *decoder_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    t_decoder_tilde *x = (t_decoder_tilde *)pd_new(decoder_tilde_class);
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

    x->order = (int)floor(sqrt(num_loudspeakers) - 1);
    x->nSH = (order + 1) * (order + 1);
    x->num_loudspeakers = num_loudspeakers;
    if (x->order < 1) {
        pd_error(x, "[saf.decoder~] Minimal order is 1 (requires at least 4 loudspeakers). "
                    "Falling back to binaural mode.");
    }

    ambi_dec_create(&x->hAmbi);

    // if (x->num_loudspeakers == 2) {
    //     ambi_dec_setoutputconfigpreset(x->hambi, loudspeaker_array_preset_stereo);
    // } else if (x->num_loudspeakers == 3) {
    //     ambi_dec_setoutputconfigpreset(x->hambi, loudspeaker_array_preset_stereo);
    // } else if (x->num_loudspeakers == 4) {
    //     ambi_dec_setoutputconfigpreset(x->hambi, loudspeaker_array_preset_t_design_4);
    // } else if (x->num_loudspeakers == 5) {
    //     ambi_dec_setoutputconfigpreset(x->hambi, loudspeaker_array_preset_5px);
    // } else if (x->num_loudspeakers == 22) {
    //     ambi_dec_setoutputconfigpreset(x->hambi, loudspeaker_array_preset_22px);
    // }

    for (int i = 1; i < x->nSH; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->num_loudspeakers; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    return (void *)x;
}

// ─────────────────────────────────────
void decoder_tilde_free(t_decoder_tilde *x) {
    ambi_dec_destroy(&x->hAmbi);
    if (x->ins) {
        for (int i = 0; i < x->nSH; i++) {
            freebytes(x->ins[i], x->ambiFrameSize * sizeof(t_sample));
            freebytes(x->ins_tmp[i], x->ambiFrameSize * sizeof(t_sample));
        }
        freebytes(x->ins, x->nSH * sizeof(t_sample *));
        freebytes(x->ins_tmp, x->nSH * sizeof(t_sample));
    }

    if (x->outs) {
        for (int i = 0; i < x->num_loudspeakers; i++) {
            freebytes(x->outs[i], x->ambiFrameSize * sizeof(t_sample));
            freebytes(x->outs_tmp[i], x->ambiFrameSize * sizeof(t_sample));
        }
        freebytes(x->ins, x->num_loudspeakers * sizeof(t_sample *));
        freebytes(x->outs_tmp, x->num_loudspeakers * sizeof(t_sample *));
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
}
