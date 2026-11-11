/*
 * lxr_port.h: stand-ins for the STM32 and LXR firmware services the LXR DSP code calls.
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of Delxr's port of the LXR drum voice. It is licensed under the GNU
 * General Public License version 3, with the additional permission and terms in NOTICE.md
 * alongside this file.
 */

#ifndef LXR_PORT_H_
#define LXR_PORT_H_

#include <math.h>
#include <stdint.h>

/* CMSIS intrinsics used by the LXR DSP code, in portable C. */
static inline int32_t lxr_ssat(int32_t value, int bits) {
	const int32_t max = (1 << (bits - 1)) - 1;
	const int32_t min = -(1 << (bits - 1));
	return value > max ? max : (value < min ? min : value);
}
#define __SSAT(value, bits) lxr_ssat((value), (bits))

/* Saturating add/subtract of two packed pairs of signed 16-bit values. */
static inline uint32_t __QADD16(uint32_t a, uint32_t b) {
	const int32_t lo = lxr_ssat((int16_t)a + (int16_t)b, 16);
	const int32_t hi = lxr_ssat((int16_t)(a >> 16) + (int16_t)(b >> 16), 16);
	return ((uint32_t)hi << 16) | (uint16_t)lo;
}
static inline uint32_t __QSUB16(uint32_t a, uint32_t b) {
	const int32_t lo = lxr_ssat((int16_t)a - (int16_t)b, 16);
	const int32_t hi = lxr_ssat((int16_t)(a >> 16) - (int16_t)(b >> 16), 16);
	return ((uint32_t)hi << 16) | (uint16_t)lo;
}
#define __CLZ(value) ((value) ? (uint32_t)__builtin_clz(value) : 32u)

/* The LXR reads a hardware random number generator; Delxr uses a small software one. */
uint32_t GetRngValue(void);

/* From the LXR firmware's MidiParser.c and sequencer.h. */
float midiParser_calcDetune(uint8_t value);
#define SEQ_DEFAULT_NOTE 60

#endif /* LXR_PORT_H_ */
