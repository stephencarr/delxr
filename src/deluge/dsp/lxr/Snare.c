/*
 * Snare.c
 * Created on: 17.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in NOTICE.md.
 * Modified for Delxr on 2026-11-11: one explicit instance replaces the global voice;
 * Deluge supplies LFOs, pan, velocity modulation and fader. The original signal path
 * remains: pitched noise -> SVF, mix tonal oscillator and transient, amp EG, distortion.
 * Initialization is deterministic; negative envelope overshoot is clamped at the VCA
 * and the final mix saturates instead of overflowing int16_t. See README.md.
 */
#include "Snare.h"
#include <string.h>

void Snare_init(SnareVoice* voice) {
	memset(voice, 0, sizeof(*voice));
	SnapEg_init(&voice->snapEg);
	voice->vol = 0.8f;
	voice->noiseOsc.freq = 440.f;
	voice->noiseOsc.waveform = NOISE;
	voice->noiseOsc.midiFreq = (70 << 8) | 64;
	voice->noiseOsc.pitchMod = 1.f;
	voice->noiseOsc.modNodeValue = 1.f;
	voice->osc.freq = 440.f;
	voice->osc.waveform = TRI;
	voice->osc.midiFreq = (70 << 8) | 64;
	voice->osc.pitchMod = 1.f;
	voice->osc.modNodeValue = 1.f;
	setDistortionShape(&voice->distortion, 0.f);
	transient_init(&voice->transGen);
	DecayEg_init(&voice->oscPitchEg);
	DecayEg_setSlope(&voice->oscPitchEg, 32);
	voice->egPitchModAmount = 0.5f;
	slopeEg2_init(&voice->oscVolEg);
	SVF_init(&voice->filter);
	voice->filterType = FILTER_BP;
}

void Snare_trigger(SnareVoice* voice, uint8_t note) {
	float offset = 1.f;
	if (voice->transGen.waveform == 1) offset -= voice->transGen.volume;
	if (voice->osc.waveform == SINE)
		voice->osc.phase = (0x3ff << 20) * offset;
	else if (voice->osc.waveform > SINE && voice->osc.waveform <= REC)
		voice->osc.phase = (0xff << 22) * offset;
	else
		voice->osc.phase = 0;
	DecayEg_trigger(&voice->oscPitchEg);
	slopeEg2_trigger(&voice->oscVolEg);
	osc_setBaseNote(&voice->osc, note);
	transient_trigger(&voice->transGen);
	SnapEg_trigger(&voice->snapEg);
}

void Snare_calcAsync(SnareVoice* voice) {
	const float egPitchVal = DecayEg_calc(&voice->oscPitchEg);
	voice->osc.pitchMod = 1.f + egPitchVal * voice->egPitchModAmount;
	voice->egValueOscVol = fmaxf(0.f, slopeEg2_calc(&voice->oscVolEg));
	if (voice->transGen.waveform == 0)
		voice->osc.pitchMod += SnapEg_calc(&voice->snapEg, voice->transGen.pitch) * voice->transGen.volume;
	osc_setFreq(&voice->osc);
	osc_setFreq(&voice->noiseOsc);
}

void Snare_calcSyncBlock(SnareVoice* voice, int16_t* buf, uint8_t size) {
	int16_t transBuf[OUTPUT_DMA_SIZE];
	int16_t oscBuf[OUTPUT_DMA_SIZE];
	calcNoiseBlock(&voice->noiseOsc, buf, size, 0.9f);
	SVF_calcBlockZDF(&voice->filter, voice->filterType, buf, size);
	calcNextOscSampleBlock(&voice->osc, oscBuf, size, 1.f - voice->mix, voice->pwm);
	transient_calcBlock(&voice->transGen, transBuf, size);
	const float gain = voice->vol * voice->egValueOscVol;
	for (uint8_t j = 0; j < size; ++j) {
		const float sample = gain * (fmaf(buf[j], voice->mix, oscBuf[j]) + transBuf[j]);
		buf[j] = (int16_t)fmaxf(-32768.f, fminf(32767.f, sample));
	}
	calcDistBlock(&voice->distortion, buf, size);
}
