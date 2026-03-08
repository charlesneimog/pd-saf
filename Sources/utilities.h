#ifndef SAF_UTILITIES_H
#define SAF_UTILITIES_H

#include <stdlib.h>
#include <m_pd.h>
#include <math.h>

#include <_common.h>

// ─────────────────────────────────────
#define pd_assert(pd_obj, condition, message)                                                      \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            pd_error(pd_obj, message);                                                             \
            return;                                                                                \
        }                                                                                          \
    } while (0)

// ─────────────────────────────────────
static void init_logcallback(t_pd *obj, void *data) {
    const char *msg = (const char *)data;
    logpost(obj, 2, "%s", msg);
    free((void *)msg);
}

// ─────────────────────────────────────
static inline int get_ambisonic_order(int nchs) {
    int order = (int)floor(sqrt((double)nchs) - 1.0);
    return order;
}

// ─────────────────────────────────────
static inline int get_source_config_preset(int num_inputs) {
    switch (num_inputs) {
    case 1:
        return SOURCE_CONFIG_PRESET_MONO;
    case 2:
        return SOURCE_CONFIG_PRESET_STEREO;
    case 5:
        return SOURCE_CONFIG_PRESET_5PX;
    case 7:
        return SOURCE_CONFIG_PRESET_7PX;
    case 8:
        return SOURCE_CONFIG_PRESET_8PX;
    case 9:
        return SOURCE_CONFIG_PRESET_9PX;
    case 10:
        return SOURCE_CONFIG_PRESET_10PX;
    case 11:
        return SOURCE_CONFIG_PRESET_11PX;
    case 13:
        return SOURCE_CONFIG_PRESET_13PX;
    case 22:
        return SOURCE_CONFIG_PRESET_22PX;
    case 32:
        return SOURCE_CONFIG_PRESET_22P2_9_10_3;
    case 50:
        return SOURCE_CONFIG_PRESET_AALTO_MCC;
    case 51:
        return SOURCE_CONFIG_PRESET_AALTO_MCC_SUBSET;
    case 52:
        return SOURCE_CONFIG_PRESET_AALTO_APAJA;
    case 53:
        return SOURCE_CONFIG_PRESET_AALTO_LR;
    case 54:
        return SOURCE_CONFIG_PRESET_DTU_AVIL;
    case 55:
        return SOURCE_CONFIG_PRESET_ZYLIA_LAB;
    case 4:
        return SOURCE_CONFIG_PRESET_T_DESIGN_4;
    case 12:
        return SOURCE_CONFIG_PRESET_T_DESIGN_12;
    case 24:
        return SOURCE_CONFIG_PRESET_T_DESIGN_24;
    case 36:
        return SOURCE_CONFIG_PRESET_T_DESIGN_36;
    case 48:
        return SOURCE_CONFIG_PRESET_T_DESIGN_48;
    case 60:
        return SOURCE_CONFIG_PRESET_T_DESIGN_60;
    case 16:
        return SOURCE_CONFIG_PRESET_SPH_COV_16;
    case 25:
        return SOURCE_CONFIG_PRESET_SPH_COV_25;
    case 49:
        return SOURCE_CONFIG_PRESET_SPH_COV_49;
    case 64:
        return SOURCE_CONFIG_PRESET_SPH_COV_64;
    default:
        return -1; // Default preset
    }
}

// ─────────────────────────────────────
static inline int get_min_speakers(int preset) {
    switch (preset) {
    default:
    case LOUDSPEAKER_ARRAY_PRESET_DEFAULT:
    case LOUDSPEAKER_ARRAY_PRESET_5PX:
        return 5;

    case LOUDSPEAKER_ARRAY_PRESET_7PX:
        return 7;

    case LOUDSPEAKER_ARRAY_PRESET_8PX:
        return 8;

    case LOUDSPEAKER_ARRAY_PRESET_9PX:
        return 9;

    case LOUDSPEAKER_ARRAY_PRESET_10PX:
        return 10;

    case LOUDSPEAKER_ARRAY_PRESET_11PX:
    case LOUDSPEAKER_ARRAY_PRESET_11PX_7_4:
        return 11;

    case LOUDSPEAKER_ARRAY_PRESET_13PX:
    case LOUDSPEAKER_ARRAY_PRESET_AALTO_LR:
        return 13;

    case LOUDSPEAKER_ARRAY_PRESET_22PX:
    case LOUDSPEAKER_ARRAY_PRESET_ZYLIA_LAB:
        return 22;

    case LOUDSPEAKER_ARRAY_PRESET_22P2_9_10_3:
        return -1; /* invalid, contains LFE */

    case LOUDSPEAKER_ARRAY_PRESET_AALTO_MCC:
        return 45;

    case LOUDSPEAKER_ARRAY_PRESET_AALTO_MCC_SUBSET:
        return 37;

    case LOUDSPEAKER_ARRAY_PRESET_AALTO_APAJA:
        return 29;

    case LOUDSPEAKER_ARRAY_PRESET_DTU_AVIL:
        return 64;

    case LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_4:
        return 4;

    case LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_12:
        return 12;

    case LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_24:
        return 24;

    case LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_36:
        return 36;

    case LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_48:
        return 48;

    case LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_60:
        return 60;

    case LOUDSPEAKER_ARRAY_PRESET_SPH_COV_9:
        return 9;

    case LOUDSPEAKER_ARRAY_PRESET_SPH_COV_16:
        return 16;

    case LOUDSPEAKER_ARRAY_PRESET_SPH_COV_25:
        return 25;

    case LOUDSPEAKER_ARRAY_PRESET_SPH_COV_49:
        return 49;

    case LOUDSPEAKER_ARRAY_PRESET_SPH_COV_64:
        return 64;
    }
}

// ─────────────────────────────────────
static inline int get_loudspeaker_array_preset(int num_outputs) {
    switch (num_outputs) {
    case 2:
        return LOUDSPEAKER_ARRAY_PRESET_STEREO;
    case 5:
        return LOUDSPEAKER_ARRAY_PRESET_5PX;
    case 7:
        return LOUDSPEAKER_ARRAY_PRESET_7PX;
    case 8:
        return LOUDSPEAKER_ARRAY_PRESET_8PX;
    case 9:
        return LOUDSPEAKER_ARRAY_PRESET_9PX;
    case 10:
        return LOUDSPEAKER_ARRAY_PRESET_10PX;
    case 11:
        return LOUDSPEAKER_ARRAY_PRESET_11PX;
    case 12:
        return LOUDSPEAKER_ARRAY_PRESET_11PX_7_4;
    case 13:
        return LOUDSPEAKER_ARRAY_PRESET_13PX;
    case 22:
        return LOUDSPEAKER_ARRAY_PRESET_22PX;
    case 32:
        return LOUDSPEAKER_ARRAY_PRESET_22P2_9_10_3;
    case 50:
        return LOUDSPEAKER_ARRAY_PRESET_AALTO_MCC;
    case 51:
        return LOUDSPEAKER_ARRAY_PRESET_AALTO_MCC_SUBSET;
    case 52:
        return LOUDSPEAKER_ARRAY_PRESET_AALTO_APAJA;
    case 53:
        return LOUDSPEAKER_ARRAY_PRESET_AALTO_LR;
    case 54:
        return LOUDSPEAKER_ARRAY_PRESET_DTU_AVIL;
    case 55:
        return LOUDSPEAKER_ARRAY_PRESET_ZYLIA_LAB;
    case 4:
        return LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_4;
    case 24:
        return LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_24;
    case 36:
        return LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_36;
    case 48:
        return LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_48;
    case 60:
        return LOUDSPEAKER_ARRAY_PRESET_T_DESIGN_60;
    case 16:
        return LOUDSPEAKER_ARRAY_PRESET_SPH_COV_16;
    case 25:
        return LOUDSPEAKER_ARRAY_PRESET_SPH_COV_25;
    case 49:
        return LOUDSPEAKER_ARRAY_PRESET_SPH_COV_49;
    case 64:
        return LOUDSPEAKER_ARRAY_PRESET_SPH_COV_64;
    default:
        return -1; // Default preset
    }
}

#endif
