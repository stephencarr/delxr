/*
 * delxr_lxr_drum.c: drives one LXR DrumVoice from Delxr.
 *
 * The LXR engine renders fixed blocks of 64 samples and advances its envelopes once per block.
 * The Deluge asks for blocks of any size, so each voice renders 64 samples at a time into its own
 * small buffer and hands them out as requested. Envelope timing is then exactly the LXR's.
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-11.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of Delxr's port of the LXR drum voice. It is licensed under the GNU
 * General Public License version 3, with the additional permission and terms in NOTICE.md
 * alongside this file.
 */

#include "delxr_lxr_drum.h"
#include "DrumVoice.h"

typedef struct {
	DrumVoice voice;
	int16_t block[OUTPUT_DMA_SIZE];
	uint8_t blockPos;  /* next unread sample in block; OUTPUT_DMA_SIZE means empty */
	uint8_t finished;
} Instance;

_Static_assert(sizeof(Instance) <= sizeof(DelxrLxrDrum), "raise DELXR_LXR_DRUM_BYTES");

static uint8_t toMidiValue(float value, float max) {
	if (!isfinite(value)) value = 0.f;
	const float scaled = value * max;
	return scaled <= 0.f ? 0 : (scaled >= max ? (uint8_t)max : (uint8_t)scaled);
}

/* The LXR's parameter scaling, as its MidiParser.c applies it to incoming control values. */
static void applyControls(DrumVoice* voice, const DelxrLxrDrumControls* controls) {
	const float pitchAmount = isfinite(controls->pitchAmount)
	    ? fminf(1.f, fmaxf(0.f, controls->pitchAmount)) : 0.f;
	const float drive = isfinite(controls->drive)
	    ? fminf(1.f, fmaxf(0.f, controls->drive)) : 0.f;
	/* 120 of 127 keeps the longest decay to about 25 seconds; at 127 the LXR envelope never ends */
	slopeEg2_setDecay(&voice->oscVolEg, toMidiValue(controls->decay, 120.f), AMP_EG_SYNC);
	voice->egPitchModAmount = pitchAmount * pitchAmount * PITCH_AMOUNT_FACTOR;
	DecayEg_setDecay(&voice->oscPitchEg, toMidiValue(controls->pitchDecay, 127.f));
	setDistortionShape(&voice->distortion, drive);
}

void delxr_lxr_drum_trigger(DelxrLxrDrum* drum, const DelxrLxrDrumControls* controls) {
	if (!drum || !controls) return;
	Instance* instance = (Instance*)drum;
	DrumVoice* voice = &instance->voice;
	initDrumVoice(voice);

	/* A plain kick patch. These fixed values are Delxr's choice, not an LXR factory preset. */
	voice->osc.waveform = SINE;
	voice->osc.midiFreq = (33 << 8) | 64; /* coarse tune: A1, 55 Hz */
	voice->modOsc.waveform = SINE;
	voice->modOsc.midiFreq = (33 << 8) | 64;
	voice->fmModAmount = 0.f;
	voice->mixOscs = 1;
	voice->vol = 1.f;
	voice->volumeMod = 0; /* the Deluge applies velocity itself */
	slopeEg2_setAttack(&voice->oscVolEg, 0, AMP_EG_SYNC);
	slopeEg2_setSlope(&voice->oscVolEg, 32);
	DecayEg_setSlope(&voice->oscPitchEg, 32);
	transient_setWaveform(&voice->transGen, 0); /* 0 = "snap": a short upward pitch click */
	voice->transGen.volume = 0.3f;
	voice->transGen.pitch = 1.f;
	voice->filterType = FILTER_LP;
	SVF_directSetFilterValue(&voice->filter, 1.f); /* fully open */
	SVF_setReso(&voice->filter, 0.f);
	SVF_setDrive(&voice->filter, 0.316f); /* unity gain into the filter */
	applyControls(voice, controls);

	/* On the LXR a voice lives forever, so its parameter smoothers are always settled. A voice
	 * here is new for each note; start the smoothers at their targets so it does not fade in. */
	setOnePoleValue(&voice->MixerPageFilter[0], voice->vol);
	setOnePoleValue(&voice->distortion.DriveParamFilter, voice->distortion.shape);

	Drum_trigger(voice, 127, SEQ_DEFAULT_NOTE);
	instance->blockPos = OUTPUT_DMA_SIZE;
	instance->finished = 0;
}

int delxr_lxr_drum_render(DelxrLxrDrum* drum, const DelxrLxrDrumControls* controls, float pitchRatio, int16_t* out,
                          int32_t numSamples) {
	if (!drum || !controls || !out || numSamples <= 0) return 0;
	pitchRatio = isfinite(pitchRatio) ? fminf(32.f, fmaxf(0.03125f, pitchRatio)) : 1.f;
	Instance* instance = (Instance*)drum;
	DrumVoice* voice = &instance->voice;
	applyControls(voice, controls);
	voice->osc.modNodeValue = pitchRatio;
	voice->modOsc.modNodeValue = pitchRatio;

	for (int32_t i = 0; i < numSamples; i++) {
		if (instance->blockPos >= OUTPUT_DMA_SIZE) {
			calcDrumVoiceAsync(voice);
			calcDrumVoiceSyncBlock(voice, instance->block, OUTPUT_DMA_SIZE);
			instance->blockPos = 0;
			/* a block rendered entirely at zero gain means the amplitude envelope has ended */
			if (voice->oscVolEg.state == EG_D && voice->ampFilterInput <= 0. && voice->lastGain <= 0.) {
				instance->finished = 1;
			}
		}
		out[i] = instance->block[instance->blockPos++];
	}
	return !instance->finished;
}
