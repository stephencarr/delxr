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
#include "delxr_lxr_patch.h"
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
	float tone;   /* Noise filter cutoff: native full range with patch; legacy 500..10000 Hz. */
	float snappy; /* Native mix: zero = tonal oscillator; one = filtered noise. */
	float clap;   /* Zero = no repeats; positive = 1..4 native envelope repeats. */
} DelxrLxrSnareControls;

void delxr_lxr_snare_trigger(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls);
/* Arbitrary sample count, native 64-sample buffering. Returns zero after the tail is consumed.
 * Pitch ratio 1 is the patch's G3; positive finite ratios clamp to 1/64..16.
 * Clap repeat count is captured at trigger; repeat spacing follows live control changes. */
int delxr_lxr_snare_render(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                          float pitchRatio, int16_t* out, int32_t numSamples);
/* Native LXR sound design. NULL uses the original four-control patch unchanged.
 * Extra settings are read at the next native block; they do not restart envelopes.
 * Amp attack >0 also sets the native clap repeat decay time. At zero, clap retains
 * its original coupled repeat time (or instantaneous attack when clap is off).
 * Repeat count is captured at trigger. Filter processing affects the noise branch only.
 * Noise clock uses the source's linear CC/127 *22000 Hz; zero holds its last sample.
 * Tone uses the source's full-range filter shaper (0..0.45 times the sample rate).
 * Transient Offset sets start phase on trigger; sampled transients remain one-shots. */
void delxr_lxr_snare_trigger_patch(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                                  const DelxrLxrPatch* patch);
int delxr_lxr_snare_render_patch(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                                 float pitchRatio, int16_t* out, int32_t numSamples,
                                 const DelxrLxrPatch* patch);
#ifdef __cplusplus
}
#endif
#endif
