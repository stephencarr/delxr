/*
 * Snare.h
 *
 *  Created on: 16.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md.
 *
 * Modified for Delxr on 2026-11-12. The LXR firmware has one global snare voice; here every function
 * takes a pointer to one voice, so any number of independent voices can exist. Removed, because Delxr
 * supplies them per voice elsewhere or the Deluge has its own: the LFO member, panning, the velocity
 * modulation nodes and the panel fader. The saturating add of the transient is written per sample
 * instead of on packed pairs (same result). The signal path is unchanged. NOTICE.md is alongside this
 * file; see README.md there for provenance.
 */


#ifndef SNARE_H_
#define SNARE_H_

#include <stdint.h>
#include "Oscillator.h"
#include "config.h"
#include "ResonantFilter.h"
#include "distortion.h"
#include "Decay.h"
#include "SlopeEg2.h"
#include "transientGenerator.h"
#include "BufferTools.h"
#include "snapEg.h"
#include "1PoleLp.h"

//------------------------------------------------------------------------
typedef struct SnareStruct
{
	OscInfo 	osc;		// the tonal oscillator
	OscInfo 	noiseOsc;	// the noise osc


	uint8_t		filterType; // bit 0 = lp, bit 1 = hp, bit 3 = bp on/off
	float	 	vol;		// volume of the voice
	//float		panL;		// [0:1]
	//float		panR;		// [0:1]
	uint8_t pan;
	float 	triggeredPanL; //only updated with the latest pan value when triggered
	float 	triggeredPanR;
	//float 		panModifier;

	float   	mix;		// defines the mix between osc and noise [0 = 100% osc: 1 = 100% noise]
	float 		velo;

	int16_t		oscSample;
	int32_t		noiseSample;

#if USE_PEAK
	PeakFilter  filter;
#else
	ResonantFilter filter;
#endif

	DecayEg		oscPitchEg;
	float		egPitchModAmount;
	TransientGenerator transGen;

	SlopeEg2		oscVolEg;
	float 		egValueOscVol;

	Distortion distortion;
	SnapEg snapEg;

	uint8_t volumeMod;	//modulate volume by midi velocity if 1

	float combinedVolume;
	OnePoleFilter FaderVolumeFilter;

	float pwm;

} SnareVoice;
//------------------------------------------------------------------------
void Snare_init(SnareVoice* voice);
void Snare_trigger(SnareVoice* voice, const uint8_t vel, const uint8_t note);
void Snare_calcAsync(SnareVoice* voice);
void Snare_calcSyncBlock(SnareVoice* voice, int16_t* buf, const uint8_t size);

#endif /* SNARE_H_ */
