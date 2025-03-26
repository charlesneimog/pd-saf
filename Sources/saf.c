#include <m_pd.h>

static t_class *saf_class;

extern void setup_saf0x2edecoder_tilde(void);
extern void setup_saf0x2eencoder_tilde(void);
extern void setup_saf0x2ebinaural_tilde(void);

// ─────────────────────────────────────
typedef struct _saf {
    t_object obj;
} t_saf;

// ─────────────────────────────────────
void *saf_new() {
    //
    t_saf *x = (t_saf *)pd_new(saf_class);
    return (void *)x;
}

// ─────────────────────────────────────
void setup_saf0x2eencoder_tilde(void) {
    saf_class =
        class_new(gensym("saf"), (t_newmethod)saf_new, NULL, sizeof(t_saf), CLASS_DEFAULT, 0);

    post("[saf] is a pd version of Spatial Audio Framework by Leo McCormack");
    post("[saf] pd-saf by Charles K. Neimog");

    setup_saf0x2edecoder_tilde();
    setup_saf0x2eencoder_tilde();
    setup_saf0x2ebinaural_tilde();
}
