/*
 * Delxr wrapper for LXR CymbalVoice and HiHat. Source DSP remains in its own files.
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11. SPDX-License-Identifier: GPL-3.0-only
 * Licensed under GNU GPL version 3 with the additional terms in NOTICE.md.
 */
#include "delxr_lxr_metal.h"
#include "CymbalVoice.h"
#include "HiHat.h"
#include <string.h>

typedef struct {
    union {
        CymbalVoice cymbal;
        HiHatVoice hat;
    } voice;
    int16_t block[OUTPUT_DMA_SIZE];
    uint8_t blockPos;
    uint8_t finished;
    DelxrLxrMetalType type;
} Instance;
_Static_assert(sizeof(Instance) <= sizeof(DelxrLxrMetal), "raise DELXR_LXR_METAL_BYTES");

static float normalized(float value, float fallback) {
    if (!isfinite(value)) return fallback;
    return value < 0.f ? 0.f : (value > 1.f ? 1.f : value);
}

/* These are Delxr patches, not claims of LXR factory presets. The original CY/HH
 * DSP differs in carrier gain and hat decay selection. Delxr additionally gives
 * hats a higher carrier and maps their decays to separate musically useful ranges. */
static void applyControls(Instance* instance, const DelxrLxrMetalControls* controls, float pitch,
                          const DelxrLxrPatch* patch) {
    const float decay = normalized(controls->decay, .45f);
    const float fm = normalized(controls->fmAmount, .6f);
    const float ratio = normalized(controls->ratio, .5f);
    const float tone = normalized(controls->tone, .6f);
    const uint8_t decayMin = instance->type == DELXR_LXR_CYMBAL ? 40 :
                             (instance->type == DELXR_LXR_OPEN_HAT ? 20 : 1);
    const uint8_t decayMax = instance->type == DELXR_LXR_CYMBAL ? 115 :
                             (instance->type == DELXR_LXR_OPEN_HAT ? 100 : 32);
    const uint8_t midiDecay = decayMin + (uint8_t)(decay * (decayMax - decayMin));
    if (!isfinite(pitch) || pitch <= 0.f) pitch = 1.f;
    if (pitch < 1.f / 64.f) pitch = 1.f / 64.f;
    if (pitch > 16.f) pitch = 16.f;

    OscInfo *osc, *mod, *mod2;
    ResonantFilter* filter;
    SlopeEg2* envelope;
    TransientGenerator* transient;
    Distortion* distortion;
    float *pwm, *fm1, *fm2;
    uint8_t* filterType;
    if (instance->type == DELXR_LXR_CYMBAL) {
        CymbalVoice* voice = &instance->voice.cymbal;
        osc = &voice->osc; mod = &voice->modOsc; mod2 = &voice->modOsc2;
        filter = &voice->filter; envelope = &voice->oscVolEg;
        transient = &voice->transGen; distortion = &voice->distortion;
        pwm = &voice->pwm; fm1 = &voice->fmModAmount1; fm2 = &voice->fmModAmount2;
        filterType = &voice->filterType;
        voice->fmModAmount1 = voice->fmModAmount2 = fm;
    } else {
        HiHatVoice* voice = &instance->voice.hat;
        osc = &voice->osc; mod = &voice->modOsc; mod2 = &voice->modOsc2;
        filter = &voice->filter; envelope = &voice->oscVolEg;
        transient = &voice->transGen; distortion = &voice->distortion;
        pwm = &voice->pwm; fm1 = &voice->fmModAmount1; fm2 = &voice->fmModAmount2;
        filterType = &voice->filterType;
        voice->fmModAmount1 = voice->fmModAmount2 = fm;
        voice->decayClosed = voice->decayOpen = slopeEg2_calcDecay(midiDecay);
    }
    slopeEg2_setDecay(envelope, midiDecay, AMP_EG_SYNC);
    if (!patch) {
        /* Fine tuning uses the source's MIDI frequency representation. */
        const float modNote = 46.f + ratio * 60.f;
        const uint8_t coarse = (uint8_t)(modNote + .5f);
        const uint8_t fine = (uint8_t)(64 + (int)((modNote - coarse) * 127.f));
        mod->midiFreq = (coarse << 8) | fine;
        mod2->midiFreq = ((coarse + 7) << 8) | fine;
        osc_recalcFreq(mod);
        osc_recalcFreq(mod2);
    }
    if (patch) {
        /* The source MIDI parser writes these same oscillator/envelope/filter fields.
         * FM shift remains a Delxr macro: independent patch tuning is shifted together. */
        const int waves[] = {delxr_lxr_patch_get(patch, DELXR_LXR_CARRIER_WAVE),
                             delxr_lxr_patch_get(patch, DELXR_LXR_MOD1_WAVE),
                             delxr_lxr_patch_get(patch, DELXR_LXR_MOD2_WAVE)};
        OscInfo* sources[] = {osc, mod, mod2};
        for (unsigned i = 0; i < 3; ++i) {
            if (sources[i]->waveform != waves[i]) sources[i]->output = 0;
            sources[i]->waveform = waves[i];
        }
        osc->midiFreq = (delxr_lxr_patch_get(patch, DELXR_LXR_CARRIER_TUNE) << 8)
                       | delxr_lxr_patch_get(patch, DELXR_LXR_CARRIER_FINE);
        mod->midiFreq = (delxr_lxr_patch_get(patch, DELXR_LXR_MOD1_TUNE) << 8)
                       | delxr_lxr_patch_get(patch, DELXR_LXR_MOD1_FINE);
        mod2->midiFreq = (delxr_lxr_patch_get(patch, DELXR_LXR_MOD2_TUNE) << 8)
                        | delxr_lxr_patch_get(patch, DELXR_LXR_MOD2_FINE);
        osc_recalcFreq(osc);
        osc_recalcFreq(mod);
        osc_recalcFreq(mod2);
        const float shift = exp2f((ratio - .5f) * 5.f);
        mod->freq *= shift;
        mod2->freq *= shift;
        *pwm = delxr_lxr_patch_get(patch, DELXR_LXR_PULSE_WIDTH) / 127.f;
        *fm1 = fm * delxr_lxr_patch_get(patch, DELXR_LXR_MOD1_GAIN) / 127.f;
        *fm2 = fm * delxr_lxr_patch_get(patch, DELXR_LXR_MOD2_GAIN) / 127.f;
        slopeEg2_setAttack(envelope, delxr_lxr_patch_get(patch, DELXR_LXR_AMP_ATTACK), AMP_EG_SYNC);
        slopeEg2_setSlope(envelope, delxr_lxr_patch_get(patch, DELXR_LXR_AMP_SLOPE));
        envelope->repeat = instance->type == DELXR_LXR_CYMBAL ? delxr_lxr_patch_get(patch, DELXR_LXR_REPEAT) : 0;
        transient_setWaveform(transient, delxr_lxr_patch_get(patch, DELXR_LXR_TRANSIENT_WAVE));
        transient->volume = delxr_lxr_patch_get(patch, DELXR_LXR_TRANSIENT_LEVEL) / 127.f;
        transient->pitch = .25f + delxr_lxr_patch_get(patch, DELXR_LXR_TRANSIENT_TUNE) / 33.9f;
        *filterType = delxr_lxr_patch_filter_type(patch);
        SVF_setReso(filter, delxr_lxr_patch_get(patch, DELXR_LXR_FILTER_RESONANCE) / 127.f);
        SVF_setDrive(filter, delxr_lxr_patch_get(patch, DELXR_LXR_FILTER_DRIVE) / 127.f);
        setDistortionShape(distortion, delxr_lxr_patch_get(patch, DELXR_LXR_DISTORTION) / 127.f);
    }
    OscInfo* oscillators[] = {osc, mod, mod2};
    for (unsigned i = 0; i < 3; ++i) {
        /* Avoid overflow in the original oscillator's float-to-phase conversion. */
        const float maximum = 20000.f / oscillators[i]->freq;
        oscillators[i]->modNodeValue = pitch > maximum ? maximum : pitch;
    }
    SVF_directSetFilterValue(filter, patch ? delxr_lxr_filter_frequency(tone)
                                              : (300.f + 8700.f * tone * tone) / (REAL_FS * .45f));
}

void delxr_lxr_metal_trigger_patch(DelxrLxrMetal* metal, enum DelxrLxrMetalType type,
                                 const DelxrLxrMetalControls* controls, const DelxrLxrPatch* patch) {
    Instance* instance = (Instance*)metal;
    memset(instance, 0, sizeof(*instance));
    instance->type = type == DELXR_LXR_CLOSED_HAT || type == DELXR_LXR_OPEN_HAT ? type : DELXR_LXR_CYMBAL;
    SlopeEg2* envelope;
    TransientGenerator* transient;
    OscInfo* osc;
    ResonantFilter* filter;
    if (instance->type == DELXR_LXR_CYMBAL) {
        Cymbal_init(&instance->voice.cymbal);
        CymbalVoice* voice = &instance->voice.cymbal;
        envelope = &voice->oscVolEg; transient = &voice->transGen;
        osc = &voice->osc; filter = &voice->filter;
        osc->midiFreq = (70 << 8) | 64;
    } else {
        HiHat_init(&instance->voice.hat);
        HiHatVoice* voice = &instance->voice.hat;
        envelope = &voice->oscVolEg; transient = &voice->transGen;
        osc = &voice->osc; filter = &voice->filter;
        osc->midiFreq = (82 << 8) | 64;
    }
    osc_setBaseNote(osc, SEQ_DEFAULT_NOTE);
    slopeEg2_setAttack(envelope, 0, AMP_EG_SYNC);
    slopeEg2_setSlope(envelope, 32);
    transient_setWaveform(transient, 0);
    transient->volume = 0.f;
    transient->pitch = 1.f;
    SVF_setReso(filter, .1f);
    SVF_setDrive(filter, .316f);
    /* Base notes must be set before recalcFreq applies the modulator tuning. */
    if (instance->type == DELXR_LXR_CYMBAL) Cymbal_trigger(&instance->voice.cymbal, SEQ_DEFAULT_NOTE);
    else HiHat_trigger(&instance->voice.hat, instance->type == DELXR_LXR_OPEN_HAT, SEQ_DEFAULT_NOTE);
    applyControls(instance, controls, 1.f, patch);
    if (patch) {
        /* Set attack/repeat and oscillator phase offset before the real note starts.
         * The first trigger above only establishes the source's base MIDI notes. */
        if (instance->type == DELXR_LXR_CYMBAL) Cymbal_trigger(&instance->voice.cymbal, SEQ_DEFAULT_NOTE);
        else HiHat_trigger(&instance->voice.hat, instance->type == DELXR_LXR_OPEN_HAT, SEQ_DEFAULT_NOTE);
    }
    instance->blockPos = OUTPUT_DMA_SIZE;
}

int delxr_lxr_metal_render_patch(DelxrLxrMetal* metal, const DelxrLxrMetalControls* controls,
                                float pitchRatio, int16_t* out, int32_t numSamples,
                                const DelxrLxrPatch* patch) {
    Instance* instance = (Instance*)metal;
    if (numSamples <= 0) return !instance->finished;
    for (int32_t i = 0; i < numSamples; ++i) {
        if (instance->finished) {
            out[i] = 0;
            continue;
        }
        if (instance->blockPos >= OUTPUT_DMA_SIZE) {
            applyControls(instance, controls, pitchRatio, patch);
            SlopeEg2* envelope;
            if (instance->type == DELXR_LXR_CYMBAL) {
                CymbalVoice* voice = &instance->voice.cymbal;
                Cymbal_calcAsync(voice);
                Cymbal_calcSyncBlock(voice, instance->block, OUTPUT_DMA_SIZE);
                envelope = &voice->oscVolEg;
            } else {
                HiHatVoice* voice = &instance->voice.hat;
                HiHat_calcAsync(voice);
                HiHat_calcSyncBlock(voice, instance->block, OUTPUT_DMA_SIZE);
                envelope = &voice->oscVolEg;
            }
            instance->blockPos = 0;
            /* This rendered block is entirely zero, including transient/distortion. */
            if (envelope->state == EG_D && envelope->value <= 0.) instance->finished = 1;
        }
        out[i] = instance->block[instance->blockPos++];
    }
    return !instance->finished;
}

/* Keep existing callers and patches sample-identical until they opt into native settings. */
void delxr_lxr_metal_trigger(DelxrLxrMetal* metal, enum DelxrLxrMetalType type,
                           const DelxrLxrMetalControls* controls) {
    delxr_lxr_metal_trigger_patch(metal, type, controls, NULL);
}

int delxr_lxr_metal_render(DelxrLxrMetal* metal, const DelxrLxrMetalControls* controls,
                          float pitchRatio, int16_t* out, int32_t numSamples) {
    return delxr_lxr_metal_render_patch(metal, controls, pitchRatio, out, numSamples, NULL);
}
