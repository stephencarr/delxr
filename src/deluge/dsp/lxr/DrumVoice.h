/*
 * DrumVoice.h
 *
 *  Created on: 03.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md.
 *
 * Modified for Delxr on 2026-11-11. The LXR firmware keeps three drum voices in a global
 * array addressed by voice number; here every function takes a pointer to one DrumVoice, so
 * any number of independent voices can exist. Removed, because the Deluge provides its own:
 * the per-voice LFO, panning, velocity modulation nodes, the panel fader and the dither state.
 * The signal path (oscillators, pitch and amplitude envelopes, transient, filter, distortion)
 * is unchanged. NOTICE.md is alongside this file; see README.md there for provenance.
 */

#ifndef DRUMVOICE_H_
#define DRUMVOICE_H_

#include "1PoleLp.h"
#include "Decay.h"
#include "Oscillator.h"
#include "ResonantFilter.h"
#include "SlopeEg2.h"
#include "config.h"
#include "distortion.h"
#include "snapEg.h"
#include "transientGenerator.h"
//------------------------------------------------------------------------
typedef struct VoiceStruct
{
	OscInfo 	osc;		// the main fm oscillator
	OscInfo 	modOsc;		// the mod osc

	float		fmModAmount;

	float	 	vol;		// volume of the voice
	float 		velo;

	DecayEg		oscPitchEg;
	float		egPitchModAmount;

	TransientGenerator transGen;
	SlopeEg2	oscVolEg;
	Distortion distortion;

	ResonantFilter filter;
	uint8_t		filterType; // bit 0 = lp, bit 1 = hp, bit 3 = bp on/off

	double ampFilterInput;
	double lastGain;

	uint8_t mixOscs;

	SnapEg snapEg;

	uint8_t volumeMod;	//modulate volume by midi velocity if 1

	float combinedVolume;
	OnePoleFilter MixerPageFilter[2];

	float pwm;
} DrumVoice;
//------------------------------------------------------------------------

//initialize all the parameters of one voice to sane values
void initDrumVoice(DrumVoice* voice);

void Drum_trigger(DrumVoice* voice, const uint8_t vol, const uint8_t note);

/** block based calculation*/
void calcDrumVoiceSyncBlock(DrumVoice* voice, int16_t* buf, const uint8_t size);

/** calculate envelopes etc (once per block) */
void calcDrumVoiceAsync(DrumVoice* voice);

#endif /* DRUMVOICE_H_ */
