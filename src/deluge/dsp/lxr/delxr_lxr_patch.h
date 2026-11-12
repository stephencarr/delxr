/* Native LXR sound-design settings for Delxr. SPDX-License-Identifier: GPL-3.0-only
 * See NOTICE.md alongside this file for the upstream LXR additional terms. */
#ifndef DELXR_LXR_PATCH_H_
#define DELXR_LXR_PATCH_H_
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum DelxrLxrFamily {
    DELXR_LXR_FAMILY_DRUM,
    DELXR_LXR_FAMILY_SNARE,
    DELXR_LXR_FAMILY_CYMBAL,
    DELXR_LXR_FAMILY_CLOSED_HAT,
    DELXR_LXR_FAMILY_OPEN_HAT
} DelxrLxrFamily;

typedef enum DelxrLxrPatchParam {
    DELXR_LXR_CARRIER_WAVE,
    DELXR_LXR_PULSE_WIDTH,
    DELXR_LXR_MOD1_WAVE,
    DELXR_LXR_MOD1_TUNE,
    DELXR_LXR_MOD2_WAVE,
    DELXR_LXR_MOD2_TUNE,
    DELXR_LXR_MOD1_GAIN,
    DELXR_LXR_MOD2_GAIN,
    DELXR_LXR_FM_MODE,
    DELXR_LXR_AMP_ATTACK,
    DELXR_LXR_AMP_SLOPE,
    DELXR_LXR_PITCH_SLOPE,
    DELXR_LXR_PITCH_AMOUNT,
    DELXR_LXR_PITCH_DECAY,
    DELXR_LXR_TRANSIENT_WAVE,
    DELXR_LXR_TRANSIENT_LEVEL,
    DELXR_LXR_TRANSIENT_TUNE,
    DELXR_LXR_FILTER_TYPE,
    DELXR_LXR_FILTER_CUTOFF,
    DELXR_LXR_FILTER_RESONANCE,
    DELXR_LXR_FILTER_DRIVE,
    DELXR_LXR_DISTORTION,
    DELXR_LXR_NOISE_TUNE,
    DELXR_LXR_REPEAT,
    DELXR_LXR_CARRIER_TUNE,
    DELXR_LXR_CARRIER_FINE,
    DELXR_LXR_MOD1_FINE,
    DELXR_LXR_MOD2_FINE,
    DELXR_LXR_PATCH_PARAM_COUNT
} DelxrLxrPatchParam;

typedef struct DelxrLxrPatch {
    uint8_t value[DELXR_LXR_PATCH_PARAM_COUNT];
} DelxrLxrPatch;

void delxr_lxr_patch_defaults(DelxrLxrPatch* patch, DelxrLxrFamily family);
const char* delxr_lxr_patch_param_name(int index); /* stable XML name */
const char* delxr_lxr_patch_param_label(int index);
int delxr_lxr_patch_param_max(int index);
int delxr_lxr_patch_param_relevant(int index, DelxrLxrFamily family);
int delxr_lxr_patch_get(const DelxrLxrPatch* patch, int index); /* sanitized CC/enum */
const char* delxr_lxr_patch_value_name(int index, int value); /* enum labels or NULL */
/* Filter UI index0..7 maps to the source's FILTER_LP1..FILTER_OFF8. */
int delxr_lxr_patch_filter_type(const DelxrLxrPatch* patch);
/* LXR MidiParser valueShaperF2F(value, FILTER_SHAPER=-0.9). */
float delxr_lxr_filter_frequency(float normalized);

#ifdef __cplusplus
}
#endif
#endif
