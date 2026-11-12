/*
 * lxr_port.c
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of Delxr's port of the LXR drum voice. It is licensed under the GNU
 * General Public License version 3, with the additional permission and terms in NOTICE.md
 * alongside this file.
 */

#include "lxr_port.h"

/* Copied from the LXR firmware's MidiParser.c (Copyright (C) 2012-2026 Erica Synths). */
#define SEMITONE_UP 1.0594630943592952645618252949463f
float midiParser_calcDetune(uint8_t value) {
	/* linear interpolation between 1 (no change) and a semitone up/down */
	float frac = (value / 127.f - 0.5f);
	float cent = 1;
	cent += frac * (SEMITONE_UP - 1);
	return cent;
}
