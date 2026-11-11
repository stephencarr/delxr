/*
 * delxr_lxr_drum.h: the only header the rest of Delxr includes to use the LXR drum voice.
 * It deliberately exposes none of the LXR headers, whose global names and macros would collide
 * with the Deluge firmware's.
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of Delxr's port of the LXR drum voice. It is licensed under the GNU
 * General Public License version 3, with the additional permission and terms in NOTICE.md
 * alongside this file.
 */

#ifndef DELXR_LXR_DRUM_H_
#define DELXR_LXR_DRUM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Storage for one voice. The real layout is private to delxr_lxr_drum.c, which checks it fits. */
#define DELXR_LXR_DRUM_BYTES 640
typedef struct DelxrLxrDrum {
	double storage[DELXR_LXR_DRUM_BYTES / 8];
} DelxrLxrDrum;

/* The controls Delxr exposes, each from 0 to 1. */
typedef struct DelxrLxrDrumControls {
	float decay;       /* amplitude envelope decay (LXR "VELOD") */
	float pitchAmount; /* pitch envelope amount (LXR "MODAMNT") */
	float pitchDecay;  /* pitch envelope decay (LXR "PITCHD") */
	float drive;       /* distortion shape (LXR "OSC1_DIST") */
} DelxrLxrDrumControls;

/* Sets up a kick patch and starts the voice. */
void delxr_lxr_drum_trigger(DelxrLxrDrum* drum, const DelxrLxrDrumControls* controls);

/* Writes numSamples of signed 16-bit-range audio to out. pitchRatio transposes the patch (1 = as
 * patched). Returns 0 once the voice has finished sounding. */
int delxr_lxr_drum_render(DelxrLxrDrum* drum, const DelxrLxrDrumControls* controls, float pitchRatio, int16_t* out,
                          int32_t numSamples);

#ifdef __cplusplus
}
#endif

#endif /* DELXR_LXR_DRUM_H_ */
