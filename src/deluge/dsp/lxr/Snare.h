/*
 * Snare.h
 * Created on: 16.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in NOTICE.md.
 * Modified for Delxr on 2026-11-11: one explicit instance replaces the global voice;
 * Deluge supplies LFOs, pan, velocity modulation and fader. See README.md for provenance.
 */
#ifndef DELXR_SNARE_H_
#define DELXR_SNARE_H_

#include "Oscillator.h"
#include "ResonantFilter.h"
#include "distortion.h"
#include "Decay.h"
#include "SlopeEg2.h"
#include "transientGenerator.h"
#include "snapEg.h"

typedef struct SnareStruct {
	OscInfo osc;
	OscInfo noiseOsc;
	uint8_t filterType;
	float vol;
	float mix;
	ResonantFilter filter;
	DecayEg oscPitchEg;
	float egPitchModAmount;
	TransientGenerator transGen;
	SlopeEg2 oscVolEg;
	float egValueOscVol;
	Distortion distortion;
	SnapEg snapEg;
	float pwm;
} SnareVoice;

void Snare_init(SnareVoice* voice);
void Snare_trigger(SnareVoice* voice, uint8_t note);
void Snare_calcSyncBlock(SnareVoice* voice, int16_t* buf, uint8_t size);
void Snare_calcAsync(SnareVoice* voice);
#endif
