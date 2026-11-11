/*
 * BufferTools.h: the three buffer helpers the drum voice uses, from the LXR firmware's
 * BufferTools.h (Copyright (C) 2012-2026 Erica Synths). The saturating add is written per sample
 * instead of on packed pairs; the result is the same.
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of Delxr's port of the LXR drum voice. It is licensed under the GNU
 * General Public License version 3, with the additional permission and terms in NOTICE.md
 * alongside this file.
 */

#ifndef BUFFERTOOLS_H_
#define BUFFERTOOLS_H_

#include "config.h"
#include <stdint.h>

static inline void bufferTool_addBuffersSaturating(int16_t* buf1, int16_t* buf2, const uint8_t size) {
	for (uint8_t i = 0; i < size; i++) {
		buf1[i] = (int16_t)lxr_ssat((int32_t)buf1[i] + buf2[i], 16);
	}
}

static inline void bufferTool_addGain(int16_t* buf, const float gain, const uint8_t size) {
	uint8_t i;
	for (i = 0; i < size; i++) {
		buf[i] *= gain;
	}
}

static inline void bufferTool_addGainInterpolated(int16_t* buf, const float gain, const float lastGain,
                                                  const uint8_t size) {
	uint8_t i;
	for (i = 0; i < size; i++) {
		const double frac = i / (size - 1.f);
		const double currentGain = lastGain + frac * (gain - lastGain);
		buf[i] = buf[i] * currentGain;
	}
}

#endif /* BUFFERTOOLS_H_ */
