/*
 * CymbalVoice.h
 *
 *  Created on: 16.06.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md.
 *
 * Modified for Delxr on 2026-11-12. The LXR firmware has one global cymbal voice; here every function
 * takes a pointer to one voice, so any number of independent voices can exist. Removed, because Delxr
 * supplies them per voice elsewhere or the Deluge has its own: the LFO member, panning, the velocity
 * modulation nodes and the panel fader. The saturating add of the transient is written per sample
 * instead of on packed pairs (same result). The signal path is unchanged. NOTICE.md is alongside this
 * file; see README.md there for provenance.
 */


#ifndef CYMBALVOICE_H_
#define CYMBALVOICE_H_

#include <stdint.h>
#include "Oscillator.h"
#include "config.h"
#include "ResonantFilter.h"
#include "SlopeEg2.h"
#include "transientGenerator.h"
#include "BufferTools.h"
#include "distortion.h"
#include "snapEg.h"
#include "1PoleLp.h"

typedef struct CymbalStruct
{

	OscInfo 	osc;		// the tonal oscillator
	OscInfo 	modOsc;		// the mod osc1
	OscInfo 	modOsc2;	// the mod osc2
	float		fmModAmount1;
	float		fmModAmount2;

	float	 	vol;		// volume of the voice
	//float		panL;		// [0:1]
	//float		panR;		// [0:1]
	uint8_t pan;
	float 	triggeredPanL; //only updated with the latest pan value when triggered
	float 	triggeredPanR;
	//float 		panModifier;

	float 		velo;

	int32_t		noiseSample;

	ResonantFilter filter;
	uint8_t		filterType;
	TransientGenerator transGen;
	Distortion distortion;

	SlopeEg2	oscVolEg;
	float 		egValueOscVol;

	uint8_t 	volumeMod;	//modulate volume by midi velocity if 1

	SnapEg 		snapEg;

	float combinedVolume;
	OnePoleFilter FaderVolumeFilter;

	float pwm;

} CymbalVoice;
//------------------------------------------------------------------------
void Cymbal_init(CymbalVoice* voice);
void Cymbal_trigger(CymbalVoice* voice, const uint8_t vel, const uint8_t note);
void Cymbal_calcAsync(CymbalVoice* voice);
void Cymbal_calcSyncBlock(CymbalVoice* voice, int16_t* buf, const uint8_t size);

#endif /* CYMBALVOICE_H_ */
