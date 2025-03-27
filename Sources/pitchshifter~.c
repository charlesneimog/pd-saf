#include <string.h>
#include <math.h>
#include <pthread.h>

#include <m_pd.h>
#include <g_canvas.h>
#include <s_stuff.h>

#include <pitch_shifter.h>

static t_class *pitchshifter_tilde_class;

// ─────────────────────────────────────
typedef struct _pitchshifter_tilde {
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
} t_pitchshifter_tilde;

// ╭─────────────────────────────────────╮
// │               Methods               │
// ╰─────────────────────────────────────╯
static void pitchshifter_tilde_set(t_pitchshifter_tilde *x, t_symbol *s, int argc, t_atom *argv) {
    const char *method = atom_getsymbol(argv)->s_name;

    if (strcmp(method, "cents") == 0) {
        float cents = atom_getfloat(argv + 1);
        float factor = pow(2, cents / 1200.0);
        post("factor is %f", factor);
        pitch_shifter_setPitchShiftFactor(x->hAmbi, factor);
    } else if (strcmp(method, "factor") == 0) {
        float factor = atom_getfloat(argv + 1);
        pitch_shifter_setPitchShiftFactor(x->hAmbi, factor);
    } else if (strcmp(method, "osamp") == 0) {
        float osamp = atom_getfloat(argv + 1);
        pitch_shifter_setOSampOption(x->hAmbi, osamp);
    } else if (strcmp(method, "fftsize") == 0) {
        // TODO: NEED TO REALLOC MEMORY THE FFT SIZE
    }
}

// ╭─────────────────────────────────────╮
// │     Initialization and Perform      │
// ╰─────────────────────────────────────╯
t_int *pitchshifter_tilde_perform(t_int *w) {
    t_pitchshifter_tilde *x = (t_pitchshifter_tilde *)(w[1]);
    int n = (int)(w[2]);

    if (n <= x->ambiFrameSize) {
        // Accumulate input samples
        for (int ch = 0; ch < x->nSH; ch++) {
            memcpy(x->ins[ch] + x->accumSize, (t_sample *)w[3 + ch], n * sizeof(t_sample));
        }
        x->accumSize += n;
        // Process only when we have a full ambisonic frame
        if (x->accumSize == x->ambiFrameSize) {
            pitch_shifter_process(x->hAmbi, (const float *const *)x->ins, (float **)x->outs, x->nSH,
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
            pitch_shifter_process(x->hAmbi, (const float *const *)x->ins_tmp, (float **)x->outs_tmp,
                                  x->nSH, x->num_loudspeakers, x->ambiFrameSize);
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
void pitchshifter_tilde_dsp(t_pitchshifter_tilde *x, t_signal **sp) {
    // Set frame sizes and reset indices
    x->ambiFrameSize = pitch_shifter_getFrameSize();
    x->pdFrameSize = sp[0]->s_n;
    x->outputIndex = 0;
    x->accumSize = 0;
    int sum = x->nSH + x->num_loudspeakers;
    int sigvecsize = sum + 2;
    t_int *sigvec = getbytes(sigvecsize * sizeof(t_int));

    // Setup multi-out signals
    for (int i = x->nSH; i < sum; i++) {
        signal_setmultiout(&sp[i], 1);
    }

    if (pitch_shifter_getProgressBar0_1(x->hAmbi) != 1 &&
        pitch_shifter_getProgressBar0_1(x->hAmbi) == 0) {
        pitch_shifter_init(x->hAmbi, sys_getsr());
        pitch_shifter_initCodec(x->hAmbi);
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

    dsp_addv(pitchshifter_tilde_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

// ─────────────────────────────────────
void *pitchshifter_tilde_new(t_symbol *s, int argc, t_atom *argv) {
    // TODO:
    t_pitchshifter_tilde *x = (t_pitchshifter_tilde *)pd_new(pitchshifter_tilde_class);
    x->glist = canvas_getcurrent();
    int order = 1;
    int num_loudspeakers = 2;
    int fft_size = 1024;

    if (argc >= 1) {
        order = atom_getint(argv);
    }
    if (argc >= 2) {
        num_loudspeakers = atom_getint(argv + 1);
    }
    if (argc >= 3) {
        fft_size = atom_getint(argv + 2);
    }

    order = order < 0 ? 0 : order;
    num_loudspeakers = num_loudspeakers < 1 ? 1 : num_loudspeakers;

    x->order = (int)floor(sqrt(num_loudspeakers) - 1);
    x->nSH = (order + 1) * (order + 1);
    x->num_loudspeakers = num_loudspeakers;
    if (x->order < 1) {
        pd_error(x, "[saf.pitchshifter~] Minimal order is 1 (requires at least 4 loudspeakers). "
                    "Falling back to binaural mode.");
    }

    pitch_shifter_create(&x->hAmbi);
    pitch_shifter_setFFTSizeOption(x->hAmbi, fft_size);
    pitch_shifter_setNumChannels(x->hAmbi, x->nSH);

    for (int i = 1; i < x->nSH; i++) {
        inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    }

    for (int i = 0; i < x->num_loudspeakers; i++) {
        outlet_new(&x->obj, &s_signal);
    }

    return (void *)x;
}

// ─────────────────────────────────────
void pitchshifter_tilde_free(t_pitchshifter_tilde *x) {
    pitch_shifter_destroy(&x->hAmbi);
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
void setup_saf0x2epitchshifter_tilde(void) {

    pitchshifter_tilde_class =
        class_new(gensym("saf.pitchshifter~"), (t_newmethod)pitchshifter_tilde_new,
                  (t_method)pitchshifter_tilde_free, sizeof(t_pitchshifter_tilde),
                  CLASS_DEFAULT | CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(pitchshifter_tilde_class, t_pitchshifter_tilde, sample);
    class_addmethod(pitchshifter_tilde_class, (t_method)pitchshifter_tilde_dsp, gensym("dsp"),
                    A_CANT, 0);
    class_addmethod(pitchshifter_tilde_class, (t_method)pitchshifter_tilde_set, gensym("set"),
                    A_GIMME, 0);
}
