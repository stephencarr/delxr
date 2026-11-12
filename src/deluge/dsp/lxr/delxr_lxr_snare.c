/*
 * Delxr wrapper for the LXR snare/clap voice.
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 * Licensed under GNU GPL version 3 with the additional permission and terms in NOTICE.md.
 */
#include "delxr_lxr_snare.h"
#include "Snare.h"
#include <string.h>

typedef struct {
	SnareVoice voice;
	int16_t block[OUTPUT_DMA_SIZE];
	uint8_t blockPos;
	uint8_t terminalBlock;
	uint8_t finished;
} Instance;

_Static_assert(sizeof(Instance) <= sizeof(DelxrLxrSnare), "raise DELXR_LXR_SNARE_BYTES");
_Static_assert(_Alignof(Instance) <= _Alignof(DelxrLxrSnare), "raise snare storage alignment");

static float normalized(float value) {
	return isfinite(value) ? fminf(1.f, fmaxf(0.f, value)) : 0.f;
}

static void applyControls(SnareVoice* voice, const DelxrLxrSnareControls* controls) {
	const float decay = normalized(controls->decay);
	const float tone = normalized(controls->tone);
	const float clap = normalized(controls->clap);
	slopeEg2_setDecay(&voice->oscVolEg, (uint8_t)(decay * 96.f), AMP_EG_SYNC);
	/* The LXR repeats its decay using the attack time, then enters its main decay.
	 * This coupled knob selects 1..4 short bursts with attack CC 7..26. */
	voice->oscVolEg.repeat = clap == 0.f ? 0 : (uint8_t)ceilf(clap * 4.f);
	slopeEg2_setAttack(&voice->oscVolEg, clap == 0.f ? 0 : (uint8_t)(7.f + clap * 19.f), AMP_EG_SYNC);
	voice->mix = normalized(controls->snappy);
	SVF_directSetFilterValue(&voice->filter, (500.f * powf(20.f, tone)) / (0.45f * REAL_FS));
}

void delxr_lxr_snare_trigger(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls) {
	Instance* instance = (Instance*)snare;
	memset(instance, 0, sizeof(*instance));
	SnareVoice* voice = &instance->voice;
	Snare_init(voice);
	voice->osc.waveform = SINE;
	voice->osc.midiFreq = (55 << 8) | 64; /* G3, approximately 196 Hz */
	voice->noiseOsc.freq = 16000.f;
	voice->noiseOsc.waveform = NOISE;
	voice->vol = 0.85f;
	voice->egPitchModAmount = 0.5f;
	DecayEg_setDecay(&voice->oscPitchEg, 24);
	slopeEg2_setSlope(&voice->oscVolEg, 24);
	transient_setWaveform(&voice->transGen, 0);
	voice->transGen.volume = 0.1f;
	voice->transGen.pitch = 1.f;
	SVF_setReso(&voice->filter, 0.25f);
	SVF_setDrive(&voice->filter, 0.316f);
	applyControls(voice, controls);
	Snare_trigger(voice, SEQ_DEFAULT_NOTE);
	instance->blockPos = OUTPUT_DMA_SIZE;
}

int delxr_lxr_snare_render(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                          float pitchRatio, int16_t* out, int32_t numSamples) {
	Instance* instance = (Instance*)snare;
	SnareVoice* voice = &instance->voice;
	if (numSamples <= 0) return !instance->finished;
	if (!isfinite(pitchRatio) || pitchRatio <= 0.f) pitchRatio = 1.f;
	pitchRatio = fminf(16.f, fmaxf(1.f / 64.f, pitchRatio));
	for (int32_t i = 0; i < numSamples; ++i) {
		if (instance->finished) {
			out[i] = 0;
			continue;
		}
		if (instance->blockPos >= OUTPUT_DMA_SIZE) {
			applyControls(voice, controls);
			voice->osc.modNodeValue = pitchRatio;
			/* Preserve pitched sample-and-hold noise while keeping its clock below Nyquist. */
			voice->noiseOsc.modNodeValue = fminf(pitchRatio, 22000.f / voice->noiseOsc.freq);
			Snare_calcAsync(voice);
			Snare_calcSyncBlock(voice, instance->block, OUTPUT_DMA_SIZE);
			instance->blockPos = 0;
			instance->terminalBlock = voice->oscVolEg.state == EG_D && voice->oscVolEg.value <= 0.;
		}
		out[i] = instance->block[instance->blockPos++];
		if (instance->terminalBlock && instance->blockPos == OUTPUT_DMA_SIZE) instance->finished = 1;
	}
	return !instance->finished;
}
