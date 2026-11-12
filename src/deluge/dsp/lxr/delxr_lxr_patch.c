/* Native LXR sound-design settings for Delxr. SPDX-License-Identifier: GPL-3.0-only
 * See NOTICE.md alongside this file for the upstream LXR additional terms. */
#include "delxr_lxr_patch.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

static const char* const names[DELXR_LXR_PATCH_PARAM_COUNT] = {
    "carrierWave", "pulseWidth", "mod1Wave", "mod1Tune", "mod2Wave", "mod2Tune",
    "mod1Gain", "mod2Gain", "fmMode", "ampAttack", "ampSlope", "pitchSlope",
    "pitchAmount", "pitchDecay", "transientWave", "transientLevel", "transientTune",
    "filterType", "filterCutoff", "filterResonance", "filterDrive", "distortion",
    "noiseTune", "repeat", "carrierTune", "carrierFine", "mod1Fine", "mod2Fine"
};
static const char* const labels[DELXR_LXR_PATCH_PARAM_COUNT] = {
    "Osc wave", "Pulse width", "FM1 wave", "FM1 coarse", "FM2 wave", "FM2 coarse",
    "FM1 amount", "FM2 amount", "FM / Mix", "Attack / rpt time", "Amp slope", "Pitch slope",
    "Pitch amount", "Pitch decay", "Transient", "Transient level", "Transient tune",
    "Filter type", "Filter cutoff", "Filter reso", "Filter drive", "Distortion",
    "Noise clock", "Repeats", "Osc coarse", "Osc fine", "FM1 fine", "FM2 fine"
};
const char* delxr_lxr_patch_param_name(int p) {
    return p >= 0 && p < DELXR_LXR_PATCH_PARAM_COUNT ? names[p] : "";
}
const char* delxr_lxr_patch_param_label(int p) {
    return p >= 0 && p < DELXR_LXR_PATCH_PARAM_COUNT ? labels[p] : "";
}
int delxr_lxr_patch_param_max(int p) {
    switch (p) {
    case DELXR_LXR_CARRIER_WAVE: case DELXR_LXR_MOD1_WAVE: case DELXR_LXR_MOD2_WAVE: return 5;
    case DELXR_LXR_FM_MODE: return 1; /* 0=FM, 1=mix */
    case DELXR_LXR_FILTER_TYPE: return 7;
    case DELXR_LXR_TRANSIENT_WAVE: return 13; /* snap, offset and twelve PCM transients */
    case DELXR_LXR_REPEAT: return 16;
    case DELXR_LXR_AMP_ATTACK: case DELXR_LXR_AMP_SLOPE:
    case DELXR_LXR_PITCH_SLOPE: case DELXR_LXR_PITCH_DECAY: return 126;
    default: return p >= 0 && p < DELXR_LXR_PATCH_PARAM_COUNT ? 127 : 0;
    }
}
int delxr_lxr_patch_get(const DelxrLxrPatch* patch, int p) {
    if (!patch || p < 0 || p >= DELXR_LXR_PATCH_PARAM_COUNT) return 0;
    const int max = delxr_lxr_patch_param_max(p);
    return patch->value[p] > max ? max : patch->value[p];
}
int delxr_lxr_patch_filter_type(const DelxrLxrPatch* patch) {
    return delxr_lxr_patch_get(patch, DELXR_LXR_FILTER_TYPE) + 1;
}
float delxr_lxr_filter_frequency(float value) {
    if (!isfinite(value)) value = 0.f;
    value = fminf(1.f, fmaxf(0.f, value));
    const float k = -1.8f / 1.9001f;
    return (1.f + k) * value / (1.f + k * fabsf(value));
}
const char* delxr_lxr_patch_value_name(int p, int value) {
    static const char* const waves[] = {"Sine", "Triangle", "Saw", "Square", "Noise", "PWM"};
    static const char* const filters[] = {"LP", "HP", "BP", "Unity BP", "Notch", "Peak", "LP2", "Off"};
    static const char* const transients[] = {"Snap", "Offset", "Click", "Click 2", "Tick", "Kick", "Rim", "Drip", "Hat", "Clap", "Kick 2", "Snare", "Tom", "Snap 2"};
    if (value < 0 || value > delxr_lxr_patch_param_max(p)) return NULL;
    if (p == DELXR_LXR_CARRIER_WAVE || p == DELXR_LXR_MOD1_WAVE || p == DELXR_LXR_MOD2_WAVE) return waves[value];
    if (p == DELXR_LXR_FILTER_TYPE) return filters[value];
    if (p == DELXR_LXR_TRANSIENT_WAVE) return transients[value];
    if (p == DELXR_LXR_FM_MODE) return value ? "Mix" : "FM";
    return NULL;
}
int delxr_lxr_patch_param_relevant(int p, DelxrLxrFamily f) {
    if (p < 0 || p >= DELXR_LXR_PATCH_PARAM_COUNT) return 0;
    const int drum = f == DELXR_LXR_FAMILY_DRUM;
    const int snare = f == DELXR_LXR_FAMILY_SNARE;
    switch (p) {
    case DELXR_LXR_MOD1_WAVE: case DELXR_LXR_MOD1_TUNE: case DELXR_LXR_MOD1_GAIN: case DELXR_LXR_MOD1_FINE: return !snare;
    case DELXR_LXR_MOD2_WAVE: case DELXR_LXR_MOD2_TUNE: case DELXR_LXR_MOD2_GAIN: case DELXR_LXR_MOD2_FINE: return !snare && !drum;
    case DELXR_LXR_FM_MODE: return drum;
    case DELXR_LXR_PITCH_SLOPE: return snare || drum;
    case DELXR_LXR_PITCH_AMOUNT: case DELXR_LXR_PITCH_DECAY: case DELXR_LXR_NOISE_TUNE: return snare;
    case DELXR_LXR_FILTER_CUTOFF: return drum;
    case DELXR_LXR_DISTORTION: return !drum;
    case DELXR_LXR_REPEAT: return f == DELXR_LXR_FAMILY_CYMBAL;
    default: return 1;
    }
}
void delxr_lxr_patch_defaults(DelxrLxrPatch* p, DelxrLxrFamily f) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->value[DELXR_LXR_PULSE_WIDTH] = 64;
    p->value[DELXR_LXR_MOD1_TUNE] = 76;
    p->value[DELXR_LXR_MOD2_TUNE] = 83;
    p->value[DELXR_LXR_MOD1_GAIN] = 127;
    p->value[DELXR_LXR_MOD2_GAIN] = 96;
    p->value[DELXR_LXR_AMP_SLOPE] = f == DELXR_LXR_FAMILY_SNARE ? 24 : 32;
    p->value[DELXR_LXR_PITCH_SLOPE] = 32;
    p->value[DELXR_LXR_PITCH_AMOUNT] = 16;
    p->value[DELXR_LXR_PITCH_DECAY] = 24;
    p->value[DELXR_LXR_TRANSIENT_TUNE] = 25;
    p->value[DELXR_LXR_FILTER_TYPE] = f == DELXR_LXR_FAMILY_DRUM ? 0 : (f == DELXR_LXR_FAMILY_SNARE ? 2 : 1);
    p->value[DELXR_LXR_FILTER_CUTOFF] = 127;
    p->value[DELXR_LXR_FILTER_RESONANCE] = f == DELXR_LXR_FAMILY_SNARE ? 32 : 13;
    p->value[DELXR_LXR_FILTER_DRIVE] = 40;
    p->value[DELXR_LXR_NOISE_TUNE] = 92; /* original noise clock CC, about 16 kHz */
    p->value[DELXR_LXR_CARRIER_TUNE] = f == DELXR_LXR_FAMILY_DRUM ? 33 : (f == DELXR_LXR_FAMILY_SNARE ? 55 : (f == DELXR_LXR_FAMILY_CYMBAL ? 70 : 82));
    p->value[DELXR_LXR_CARRIER_FINE] = p->value[DELXR_LXR_MOD1_FINE] = p->value[DELXR_LXR_MOD2_FINE] = 64;
    if (f == DELXR_LXR_FAMILY_DRUM) {
        p->value[DELXR_LXR_MOD1_GAIN] = 0;
        p->value[DELXR_LXR_MOD1_TUNE] = 33;
        p->value[DELXR_LXR_FM_MODE] = 1;
        p->value[DELXR_LXR_TRANSIENT_LEVEL] = 38;
        p->value[DELXR_LXR_FILTER_RESONANCE] = 0;
    } else if (f == DELXR_LXR_FAMILY_SNARE) {
        p->value[DELXR_LXR_TRANSIENT_LEVEL] = 13;
    } else if (f == DELXR_LXR_FAMILY_CYMBAL) {
        p->value[DELXR_LXR_CARRIER_WAVE] = 1;
    } else {
        p->value[DELXR_LXR_CARRIER_WAVE] = 3;
        p->value[DELXR_LXR_MOD1_WAVE] = 3;
        p->value[DELXR_LXR_MOD2_WAVE] = 4;
        p->value[DELXR_LXR_MOD1_GAIN] = 96;
        p->value[DELXR_LXR_MOD2_GAIN] = 127;
    }
}
