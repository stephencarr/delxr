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

static void applyControls(SnareVoice* voice, const DelxrLxrSnareControls* controls, const DelxrLxrPatch* patch) {
	const float decay = normalized(controls->decay);
	const float tone = normalized(controls->tone);
	const float clap = normalized(controls->clap);
	slopeEg2_setDecay(&voice->oscVolEg, (uint8_t)(decay * 96.f), AMP_EG_SYNC);
	/* The LXR repeats its decay using the attack time, then enters its main decay.
	 * This coupled knob selects 1..4 short bursts with attack CC 7..26. */
	voice->oscVolEg.repeat = clap == 0.f ? 0 : (uint8_t)ceilf(clap * 4.f);
	slopeEg2_setAttack(&voice->oscVolEg, clap == 0.f ? 0 : (uint8_t)(7.f + clap * 19.f), AMP_EG_SYNC);
	voice->mix = normalized(controls->snappy);
	SVF_directSetFilterValue(&voice->filter, patch ? delxr_lxr_filter_frequency(tone)
	                                            : (500.f * powf(20.f, tone)) / (0.45f * REAL_FS));
}

/* MidiParser.c's native scalings, including SNARE_NOISE_F's linear clock CC. */
static void applyPatch(SnareVoice* voice, const DelxrLxrPatch* patch) {
	if (!patch) return;
	voice->osc.waveform = delxr_lxr_patch_get(patch, DELXR_LXR_CARRIER_WAVE);
	voice->osc.midiFreq = (delxr_lxr_patch_get(patch, DELXR_LXR_CARRIER_TUNE) << 8)
	                   | delxr_lxr_patch_get(patch, DELXR_LXR_CARRIER_FINE);
	osc_setBaseNote(&voice->osc, SEQ_DEFAULT_NOTE);
	voice->pwm = delxr_lxr_patch_get(patch, DELXR_LXR_PULSE_WIDTH) / 127.f;
	voice->noiseOsc.freq = (delxr_lxr_patch_get(patch, DELXR_LXR_NOISE_TUNE) / 127.f) * 22000.f;
	const float amount = delxr_lxr_patch_get(patch, DELXR_LXR_PITCH_AMOUNT) / 127.f;
	voice->egPitchModAmount = amount * amount * PITCH_AMOUNT_FACTOR;
	DecayEg_setDecay(&voice->oscPitchEg, delxr_lxr_patch_get(patch, DELXR_LXR_PITCH_DECAY));
	DecayEg_setSlope(&voice->oscPitchEg, delxr_lxr_patch_get(patch, DELXR_LXR_PITCH_SLOPE));
	const uint8_t attack = delxr_lxr_patch_get(patch, DELXR_LXR_AMP_ATTACK);
	if (attack) slopeEg2_setAttack(&voice->oscVolEg, attack, AMP_EG_SYNC);
	slopeEg2_setSlope(&voice->oscVolEg, delxr_lxr_patch_get(patch, DELXR_LXR_AMP_SLOPE));
	transient_setWaveform(&voice->transGen, delxr_lxr_patch_get(patch, DELXR_LXR_TRANSIENT_WAVE));
	voice->transGen.volume = delxr_lxr_patch_get(patch, DELXR_LXR_TRANSIENT_LEVEL) / 127.f;
	voice->transGen.pitch = 0.25f + delxr_lxr_patch_get(patch, DELXR_LXR_TRANSIENT_TUNE) / 33.9f;
	voice->filterType = delxr_lxr_patch_filter_type(patch);
	SVF_setReso(&voice->filter, delxr_lxr_patch_get(patch, DELXR_LXR_FILTER_RESONANCE) / 127.f);
	SVF_setDrive(&voice->filter, delxr_lxr_patch_get(patch, DELXR_LXR_FILTER_DRIVE) / 127.f);
	setDistortionShape(&voice->distortion, delxr_lxr_patch_get(patch, DELXR_LXR_DISTORTION) / 127.f);
}

void delxr_lxr_snare_trigger_patch(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                                  const DelxrLxrPatch* patch) {
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
	applyControls(voice, controls, patch);
	applyPatch(voice, patch);
	Snare_trigger(voice, SEQ_DEFAULT_NOTE);
	instance->blockPos = OUTPUT_DMA_SIZE;
}

int delxr_lxr_snare_render_patch(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                                float pitchRatio, int16_t* out, int32_t numSamples,
                                const DelxrLxrPatch* patch) {
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
			applyControls(voice, controls, patch);
			applyPatch(voice, patch);
			voice->osc.modNodeValue = pitchRatio;
			/* Preserve pitched sample-and-hold noise while keeping its clock below Nyquist. */
			voice->noiseOsc.modNodeValue = voice->noiseOsc.freq > 0.f
			                                 ? fminf(pitchRatio, 22000.f / voice->noiseOsc.freq)
			                                 : 1.f;
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

void delxr_lxr_snare_trigger(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls) {
	delxr_lxr_snare_trigger_patch(snare, controls, NULL);
}

int delxr_lxr_snare_render(DelxrLxrSnare* snare, const DelxrLxrSnareControls* controls,
                          float pitchRatio, int16_t* out, int32_t numSamples) {
	return delxr_lxr_snare_render_patch(snare, controls, pitchRatio, out, numSamples, NULL);
}
