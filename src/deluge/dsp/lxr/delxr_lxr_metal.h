/*
 * Delxr wrapper for the LXR cymbal and hi-hat voices.
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11. SPDX-License-Identifier: GPL-3.0-only
 * Licensed under GNU GPL version 3 with the additional terms in NOTICE.md.
 */
#ifndef DELXR_LXR_METAL_H_
#define DELXR_LXR_METAL_H_
#include <stdint.h>
#include "delxr_lxr_patch.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DELXR_LXR_METAL_BYTES 768
typedef struct DelxrLxrMetal {
    double storage[DELXR_LXR_METAL_BYTES / 8];
} DelxrLxrMetal;

typedef enum DelxrLxrMetalType {
    DELXR_LXR_CYMBAL,
    DELXR_LXR_CLOSED_HAT,
    DELXR_LXR_OPEN_HAT
} DelxrLxrMetalType;

typedef struct DelxrLxrMetalControls {
    float decay;    /* 0..1, type-specific LXR amplitude decay range */
    float fmAmount; /* 0..1, both modulator gains */
    float ratio;    /* 0..1, linked modulator tuning over five octaves */
    float tone;    /* 0..1, cutoff: legacy 300..9000 Hz; native patch full LXR range */
} DelxrLxrMetalControls;

/* Each trigger fully resets one independent voice. The Deluge supplies hat choke groups.
 * Suggested controls for every type: { .45f, .6f, .5f, .6f }. */
void delxr_lxr_metal_trigger(DelxrLxrMetal* metal, enum DelxrLxrMetalType type,
                           const DelxrLxrMetalControls* controls);
/* Arbitrary positive block sizes. Returns zero after all audible buffered samples end.
 * Controls and pitch are sanitized; pitchRatio 1 means the patch's original tuning. */
int delxr_lxr_metal_render(DelxrLxrMetal* metal, const DelxrLxrMetalControls* controls,
                          float pitchRatio, int16_t* out, int32_t numSamples);
/* Native settings are read at the next 64-sample boundary; the pointer is not retained.
 * A NULL patch is exactly the original four-control voice. Patch FM1/FM2 coarse and
 * fine settings are independent; ratio shifts both by (ratio-.5)*60 semitones. */
void delxr_lxr_metal_trigger_patch(DelxrLxrMetal* metal, enum DelxrLxrMetalType type,
                                 const DelxrLxrMetalControls* controls, const DelxrLxrPatch* patch);
int delxr_lxr_metal_render_patch(DelxrLxrMetal* metal, const DelxrLxrMetalControls* controls,
                                float pitchRatio, int16_t* out, int32_t numSamples,
                                const DelxrLxrPatch* patch);
#ifdef __cplusplus
}
#endif
#endif
