#include <m_pd.h>

//
#include <string>

// libspatialaudio

static t_class *safEncoder;

// ─────────────────────────────────────
typedef struct _ambi {
    t_object xObj;
    t_sample sample;

    unsigned int nChIn;
    unsigned int nChOut;
    std::string SpeakerConfig;
    bool NeedSpeakersPosition = false;
    bool SpeakerPositionSet = false;

    float **ppfSpeakerFeeds;

    bool DecoderConfigured = false;
    bool Binaural = true;

    t_float azimuth;
    t_float elevation;
    t_float distance;

    t_inlet **inlets;
    t_outlet **outlets;

} elseAmbi;

// ==============================================
static t_int *AmbiPerform(t_int *w) {
    elseAmbi *x = (elseAmbi *)(w[1]);
    unsigned int n = (int)(w[2]);

    unsigned int DspArr = x->nChIn + x->nChOut + 3;

    return w + DspArr;
}

// ==============================================
static void AmbiAddDsp(elseAmbi *x, t_signal **sp) {

    unsigned int ChCount = x->nChIn + x->nChOut;

    int unsigned blockSize = sp[0]->s_n;

    t_int *SigVec = (t_int *)getbytes((ChCount + 2) * sizeof(t_int));
    SigVec[0] = (t_int)x;
    SigVec[1] = (t_int)sp[0]->s_n;

    for (int i = 0; i < ChCount; i++) {
        SigVec[i + 2] = (t_int)sp[i]->s_vec;
    }
    dsp_addv(AmbiPerform, ChCount + 2, SigVec);
    freebytes(SigVec, (ChCount + 2) * sizeof(t_int));
}

// ─────────────────────────────────────
static void *NewAmbi(t_symbol *s, int argc, t_atom *argv) {
    elseAmbi *x = (elseAmbi *)pd_new(ambi);

    // check if argv[0] and argc[1] are t_float
    // first two are n input chns and output
    if (argv[0].a_type != A_FLOAT || argv[1].a_type != A_FLOAT) {
        post("[ambi~]: invalid arguments");
        return NULL;
    }

    if (atom_getfloat(argv) != 1) {
        pd_error(x, "[ambi~]: Input channels different from 1 are not "
                    "supported yet");
        return NULL;
    }

    x->nChIn = atom_getfloat(argv);
    x->nChOut = atom_getfloat(argv + 1);

    // malloc inlets for nChIn
    x->inlets = (t_inlet **)malloc(x->nChIn * sizeof(t_inlet *));
    for (int i = 0; i < x->nChIn - 1; i++) {
        x->inlets[i] =
            inlet_new(&x->xObj, &x->xObj.ob_pd, &s_signal, &s_signal);
    }

    if (argc > 3) {
        for (int i = 3; i < argc; i++) {
            if (argv[i].a_type == A_SYMBOL) {
                std::string thing = atom_getsymbol(argv + i)->s_name;
                if (thing == "-s") { // find better name
                    i++;
                    if (argv[i].a_type == A_SYMBOL) {
                        x->SpeakerConfig = atom_getsymbol(argv + i)->s_name;
                    } else if (argv[i].a_type == A_FLOAT) {
                        x->SpeakerConfig =
                            std::to_string(atom_getfloat(argv + i));
                    }
                } else if (thing == "-b") {
                    x->Binaural = true;
                    x->nChOut = 2;
                }
            }
        }
    }

    // malloc outlets for nChOut
    x->outlets = (t_outlet **)malloc(x->nChOut * sizeof(t_outlet *));
    for (int i = 0; i < x->nChOut; i++) {
        x->outlets[i] = outlet_new(&x->xObj, &s_signal);
    }

    if (x->SpeakerConfig.empty()) {
        if (x->nChOut == 2) {
            x->SpeakerConfig = "stereo";
        } else if (x->nChOut == 4) {
            x->SpeakerConfig = "quad";
        } else {
            pd_error(x, "[ambi~]: You are using a custom speaker configuration "
                        "but you haven't specified the number of speakers. Use "
                        "[-s custom] to set.");
            return NULL;
        }
    }

    return x;
}

static void *FreeAmbi(elseAmbi *x) {

    free(x->outlets);
    return (void *)x;
}

extern "C" void ambi_tilde_setup(void);

// ─────────────────────────────────────
void saf0x2eencoder_tilde(void) {
    safEncoder =
        class_new(gensym("saf.encoder~"), (t_newmethod)NewAmbi,
                  (t_method)FreeAmbi, sizeof(elseAmbi), CLASS_DEFAULT, A_GIMME,
                  0); // maybe change to A_FLOAT AFLOAT, A_GIME

    CLASS_MAINSIGNALIN(safEncoder, elseAmbi, sample);
    class_addmethod(safEncoder, (t_method)AmbiAddDsp, gensym("dsp"), A_CANT, 0);

    // speaker, chNumber, Azi, Ele, Dis
}
