/*
 * config.h: build settings for the LXR sound engine inside Delxr.
 * Replaces the LXR firmware's Soundengine/config.h, keeping the values its DSP code depends on.
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of Delxr's port of the LXR drum voice. It is licensed under the GNU
 * General Public License version 3, with the additional permission and terms in NOTICE.md
 * alongside this file.
 */

#ifndef CONFIG_SOUNDENGINE_H_
#define CONFIG_SOUNDENGINE_H_

#include "lxr_port.h"

/* Values from the LXR firmware's config.h */
#define SMOOTH_MIXER_VOL_PAN_DRIVE 1
#define USE_BIG_SAMPLE_BUFFER 1
#define OUTPUT_DMA_SIZE 64 /* the engine's control rate: envelopes advance once per 64 samples */
#define USE_FILTER_DRIVE 0
#define AMP_EG_SYNC 0
#define ENABLE_MIX_OSC 1
#define ENABLE_DRUM_SVF 1
#define PITCH_AMOUNT_FACTOR 32
#define INTERPOLATE_OSC 1
#define INTERPOLATE_FM_OSC 1
#define USE_REVERB 0 /* the LXR FX bus is not part of Delxr */
#define USE_PEAK 0

/* The LXR runs at 44140 Hz; the Deluge at exactly 44100 Hz. */
#define REAL_FS (float)44100.f
#define REAL_FS2 (REAL_FS / 2.f)

/* STM32 memory-section placement has no meaning on the Deluge. */
#define INDTCM
#define INDTCMZ
#define IN_ITCM_RAM

#endif /* CONFIG_SOUNDENGINE_H_ */
