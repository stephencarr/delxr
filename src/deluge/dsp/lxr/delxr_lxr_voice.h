/*
 * delxr_lxr_voice.h: the only header the rest of Delxr includes to play LXR voices.
 * It exposes none of the LXR headers, whose global names and macros would collide with the
 * Deluge firmware's.
 *
 * Delxr adaptation written 2026-11-12.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Part of Delxr's port of the LXR voices. Licensed under the GNU General Public License
 * version 3, with the additional permission and terms in NOTICE.md alongside this file.
 */

#ifndef DELXR_LXR_VOICE_H_
#define DELXR_LXR_VOICE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which LXR voice family a Delxr voice plays. The values index the LXR kit's six voice slots
 * (drum 1, drum 2, drum 3, snare, cymbal, hi-hat); the hat slot plays closed or open. */
typedef enum DelxrLxrEngine {
	DELXR_LXR_DRUM1 = 0,
	DELXR_LXR_DRUM2 = 1,
	DELXR_LXR_DRUM3 = 2,
	DELXR_LXR_SNARE = 3,
	DELXR_LXR_CYMBAL = 4,
	DELXR_LXR_CLOSED_HAT = 5,
	DELXR_LXR_OPEN_HAT = 6,
} DelxrLxrEngine;

/* Number of bytes in an LXR kit's sound parameters, in the LXR's own parameter order. */
#define DELXR_LXR_KIT_BYTES 246

/* The LXR's built-in init kit, in that layout. */
const uint8_t* delxr_lxr_init_kit(void);

/* Storage for one sounding voice. The real layout is private to delxr_lxr_voice.c. */
#define DELXR_LXR_VOICE_BYTES 1024
typedef struct DelxrLxrVoice {
	double storage[DELXR_LXR_VOICE_BYTES / 8];
} DelxrLxrVoice;

/* Sets the voice up from the kit's parameters for its engine and starts it.
 * velocity is 1..127, as the LXR receives it. */
void delxr_lxr_voice_trigger(DelxrLxrVoice* voice, DelxrLxrEngine engine, const uint8_t* kit, uint8_t velocity);

/* Writes numSamples of 16-bit audio. pitchRatio transposes the kit's tuning (1 = as saved).
 * Returns 0 once the voice has finished sounding. */
int delxr_lxr_voice_render(DelxrLxrVoice* voice, float pitchRatio, int16_t* out, int32_t numSamples);

#ifdef __cplusplus
}
#endif

#endif /* DELXR_LXR_VOICE_H_ */
