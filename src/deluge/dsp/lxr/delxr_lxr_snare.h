/*
 * Delxr wrapper for the LXR snare/clap voice.
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 * Licensed under GNU GPL version 3 with the additional permission and terms in NOTICE.md.
 */
#ifndef DELXR_LXR_SNARE_H_
#define DELXR_LXR_SNARE_H_
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define DELXR_LXR_SNARE_BYTES 640
typedef struct DelxrLxrSnare {
	double storage[DELXR_LXR_SNARE_BYTES / 8];
} DelxrLxrSnare;

/* Normalized 0..1; default patch: {0.40, 0.60, 0.70, 0.0}. */
typedef struct DelxrLxrSnareControls {
	float decay;  /* Native amplitude decay CC 0..96 (maximum about 4.5 seconds). */
	float tone;   /* Noise bandpass center, exponential 500..10000 Hz. */
	float snappy; /* Native mix: zero = tonal oscillator; one = filtered noise. */
	float clap;   /* Zero = no repeats; positive = 1..4 native envelope repeats. */
} DelxrLxrSnareControls;

void delxr_lxr_snare_trigger(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls);
/* Arbitrary sample count, native 64-sample buffering. Returns zero after the tail is consumed.
 * Pitch ratio 1 is the patch's G3; positive finite ratios clamp to 1/64..16.
 * Clap repeat count is captured at trigger; repeat spacing follows live control changes. */
int delxr_lxr_snare_render(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                          float pitchRatio, int16_t* out, int32_t numSamples);
#ifdef __cplusplus
}
#endif
#endif
