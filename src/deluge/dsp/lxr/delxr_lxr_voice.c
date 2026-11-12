/*
 * delxr_lxr_voice.c: plays one LXR voice from an LXR kit's parameters.
 *
 * Copyright (C) 2012-2026 Erica Synths (LXR firmware, where noted)
 * Delxr adaptation written 2026-11-12.
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Part of Delxr's port of the LXR voices. Licensed under the GNU General Public License
 * version 3, with the additional permission and terms in NOTICE.md alongside this file.
 *
 * applyKit() sets each voice parameter exactly as the LXR firmware's MidiParser.c does when a kit
 * is loaded (preset_sendDrumsetParameters() sends every parameter through it). Panning, LFOs,
 * velocity routing and audio-output routing are not applied here.
 *
 * The LXR renders fixed 64-sample blocks and advances envelopes once per block. The Deluge asks
 * for blocks of any size, so each voice renders 64 samples at a time into its own buffer.
 */

#include "delxr_lxr_voice.h"
#include "CymbalVoice.h"
#include "DrumVoice.h"
#include "HiHat.h"
#include "InitKit.h"
#include "Parameters.h"
#include "Snare.h"
#include "valueShaper.h"
#include <string.h>

/* From the LXR firmware's globals.h and MidiParser.c. */
#define FILTER_SHAPER -0.9f
#define KIT_NAME_BYTES 8

typedef struct {
	union {
		DrumVoice drum;
		SnareVoice snare;
		CymbalVoice cymbal;
		HiHatVoice hat;
	} v;
	DelxrLxrEngine engine;
	int16_t block[OUTPUT_DMA_SIZE];
	uint8_t blockPos; /* next unread sample in block; OUTPUT_DMA_SIZE means empty */
	uint8_t finished;
	/* The LXR mixer's per-voice sample-rate reduction (mixer_decimateBlock). */
	float decimationRate;
	float decimationCount;
	int16_t decimationSample;
} Instance;

_Static_assert(sizeof(Instance) <= sizeof(DelxrLxrVoice), "raise DELXR_LXR_VOICE_BYTES");
_Static_assert(KIT_NAME_BYTES + DELXR_LXR_KIT_BYTES <= INIT_KIT_SIZE, "init kit is shorter than expected");
_Static_assert(DELXR_LXR_KIT_BYTES == PAR_MORPH_KIT_DEST, "kit layout differs from Parameters.h");

const uint8_t* delxr_lxr_init_kit(void) {
	return initKit + KIT_NAME_BYTES;
}

/* ---- Helpers with the LXR's own value scaling ---------------------------------------------- */

static float toFloat(uint8_t value) {
	return value / 127.f;
}

/* At 127 the LXR amplitude envelope never decays. LXR voices live forever, Deluge voices do not,
 * so the longest decay is held one step short of that. */
static uint8_t finiteDecay(uint8_t value) {
	return value > 126 ? 126 : value;
}

static void setCoarse(OscInfo* osc, uint8_t value) {
	osc->midiFreq &= 0x00ff;
	osc->midiFreq |= value << 8;
}

static void setFine(OscInfo* osc, uint8_t value) {
	osc->midiFreq &= 0xff00;
	osc->midiFreq |= value;
}

static void setFilterFreq(ResonantFilter* filter, uint8_t value) {
	SVF_directSetFilterValue(filter, valueShaperF2F(toFloat(value), FILTER_SHAPER));
}

static float pitchModAmount(uint8_t value) {
	const float v = toFloat(value);
	return v * v * PITCH_AMOUNT_FACTOR;
}

static float transientPitch(uint8_t value) {
	return 1.f + ((value / 33.9f) - 0.75f);
}

static float decimationRate(uint8_t value) {
	return valueShaperF2F(toFloat(value), -0.7f);
}

/* ---- Kit parameters to voice ----------------------------------------------------------------- */

static void applyDrum(Instance* instance, const uint8_t* p, int n) {
	DrumVoice* d = &instance->v.drum;
	static const uint8_t coarse[3] = {PAR_COARSE1, PAR_COARSE2, PAR_COARSE3};
	static const uint8_t fine[3] = {PAR_FINE1, PAR_FINE2, PAR_FINE3};
	static const uint8_t attack[3] = {PAR_VELOA1, PAR_VELOA2, PAR_VELOA3};
	static const uint8_t decay[3] = {PAR_VELOD1, PAR_VELOD2, PAR_VELOD3};
	static const uint8_t fmAmount[3] = {PAR_FMAMNT1, PAR_FMAMNT2, PAR_FMAMNT3};
	static const uint8_t fmTune[3] = {PAR_FM_FREQ1, PAR_FM_FREQ2, PAR_FM_FREQ3};

	d->osc.waveform = p[PAR_OSC_WAVE_DRUM1 + n];
	d->modOsc.waveform = p[PAR_MOD_WAVE_DRUM1 + n];
	setCoarse(&d->osc, p[coarse[n]]);
	setFine(&d->osc, p[fine[n]]);
	setCoarse(&d->modOsc, p[fmTune[n]]);
	d->fmModAmount = toFloat(p[fmAmount[n]]);

	setFilterFreq(&d->filter, p[PAR_FILTER_FREQ_1 + n]);
	SVF_setReso(&d->filter, toFloat(p[PAR_RESO_1 + n]));
	SVF_setDrive(&d->filter, toFloat(p[PAR_FILTER_DRIVE_1 + n]));
	d->filterType = p[PAR_FILTER_TYPE_1 + n] + 1;

	slopeEg2_setAttack(&d->oscVolEg, p[attack[n]], AMP_EG_SYNC);
	slopeEg2_setDecay(&d->oscVolEg, finiteDecay(p[decay[n]]), AMP_EG_SYNC);
	slopeEg2_setSlope(&d->oscVolEg, p[PAR_VOL_SLOPE1 + n]);
	DecayEg_setDecay(&d->oscPitchEg, p[PAR_MOD_EG1 + n]);
	DecayEg_setSlope(&d->oscPitchEg, p[PAR_PITCH_SLOPE1 + n]);
	d->egPitchModAmount = pitchModAmount(p[PAR_MODAMNT1 + n]);

	d->vol = toFloat(p[PAR_VOL1 + n]);
	setDistortionShape(&d->distortion, toFloat(p[PAR_DRIVE1 + n]));
	d->mixOscs = p[PAR_MIX_MOD_1 + n] != 0;
	d->volumeMod = p[PAR_VOLUME_MOD_ON_OFF1 + n] != 0;
	d->pwm = toFloat(p[PAR_PWM_1 + n]);

	transient_setWaveform(&d->transGen, p[PAR_TRANS1_WAVE + n]);
	d->transGen.volume = toFloat(p[PAR_TRANS1_VOL + n]);
	d->transGen.pitch = transientPitch(p[PAR_TRANS1_FREQ + n]);

	instance->decimationRate = decimationRate(p[PAR_VOICE_DECIMATION1 + n]);
}

static void applySnare(Instance* instance, const uint8_t* p) {
	SnareVoice* s = &instance->v.snare;
	s->osc.waveform = p[PAR_OSC_WAVE_SNARE];
	setCoarse(&s->osc, p[PAR_COARSE4]);
	setFine(&s->osc, p[PAR_FINE4]);
	s->noiseOsc.freq = toFloat(p[PAR_NOISE_FREQ1]) * 22000;
	s->mix = toFloat(p[PAR_MIX1]);

	setFilterFreq(&s->filter, p[PAR_FILTER_FREQ_4]);
	SVF_setReso(&s->filter, toFloat(p[PAR_RESO_4]));
	SVF_setDrive(&s->filter, toFloat(p[PAR_FILTER_DRIVE_4]));
	s->filterType = p[PAR_FILTER_TYPE_4] + 1;

	slopeEg2_setAttack(&s->oscVolEg, p[PAR_VELOA4], 0);
	slopeEg2_setDecay(&s->oscVolEg, finiteDecay(p[PAR_VELOD4]), 0);
	slopeEg2_setSlope(&s->oscVolEg, p[PAR_VOL_SLOPE4]);
	s->oscVolEg.repeat = p[PAR_REPEAT4];
	DecayEg_setDecay(&s->oscPitchEg, p[PAR_MOD_EG4]);
	DecayEg_setSlope(&s->oscPitchEg, p[PAR_PITCH_SLOPE4]);
	s->egPitchModAmount = pitchModAmount(p[PAR_MODAMNT4]);

	s->vol = toFloat(p[PAR_VOL4]);
	setDistortionShape(&s->distortion, toFloat(p[PAR_SNARE_DISTORTION]));
	s->volumeMod = p[PAR_VOLUME_MOD_ON_OFF4] != 0;
	s->pwm = toFloat(p[PAR_PWM_4]);

	transient_setWaveform(&s->transGen, p[PAR_TRANS4_WAVE]);
	s->transGen.volume = toFloat(p[PAR_TRANS4_VOL]);
	s->transGen.pitch = transientPitch(p[PAR_TRANS4_FREQ]);

	instance->decimationRate = decimationRate(p[PAR_VOICE_DECIMATION4]);
}

static void applyCymbal(Instance* instance, const uint8_t* p) {
	CymbalVoice* c = &instance->v.cymbal;
	c->osc.waveform = p[PAR_WAVE1_CYM];
	c->modOsc.waveform = p[PAR_WAVE2_CYM];
	c->modOsc2.waveform = p[PAR_WAVE3_CYM];
	setCoarse(&c->osc, p[PAR_COARSE5]);
	setFine(&c->osc, p[PAR_FINE5]);
	setCoarse(&c->modOsc, p[PAR_MOD_OSC_F1_CYM]);
	setCoarse(&c->modOsc2, p[PAR_MOD_OSC_F2_CYM]);
	c->fmModAmount1 = toFloat(p[PAR_MOD_OSC_GAIN1_CYM]);
	c->fmModAmount2 = toFloat(p[PAR_MOD_OSC_GAIN2_CYM]);

	setFilterFreq(&c->filter, p[PAR_FILTER_FREQ_5]);
	SVF_setReso(&c->filter, toFloat(p[PAR_RESO_5]));
	SVF_setDrive(&c->filter, toFloat(p[PAR_FILTER_DRIVE_5]));
	c->filterType = p[PAR_FILTER_TYPE_5] + 1;

	slopeEg2_setAttack(&c->oscVolEg, p[PAR_VELOA5], 0);
	slopeEg2_setDecay(&c->oscVolEg, finiteDecay(p[PAR_VELOD5]), 0);
	slopeEg2_setSlope(&c->oscVolEg, p[PAR_VOL_SLOPE5]);
	c->oscVolEg.repeat = p[PAR_REPEAT5];

	c->vol = toFloat(p[PAR_VOL5]);
	setDistortionShape(&c->distortion, toFloat(p[PAR_CYMBAL_DISTORTION]));
	c->volumeMod = p[PAR_VOLUME_MOD_ON_OFF5] != 0;
	c->pwm = toFloat(p[PAR_PWM_5]);

	transient_setWaveform(&c->transGen, p[PAR_TRANS5_WAVE]);
	c->transGen.volume = toFloat(p[PAR_TRANS5_VOL]);
	c->transGen.pitch = transientPitch(p[PAR_TRANS5_FREQ]);

	instance->decimationRate = decimationRate(p[PAR_VOICE_DECIMATION5]);
}

static void applyHat(Instance* instance, const uint8_t* p) {
	HiHatVoice* h = &instance->v.hat;
	h->osc.waveform = p[PAR_WAVE1_HH];
	h->modOsc.waveform = p[PAR_WAVE2_HH];
	h->modOsc2.waveform = p[PAR_WAVE3_HH];
	setCoarse(&h->osc, p[PAR_COARSE6]);
	setFine(&h->osc, p[PAR_FINE6]);
	setCoarse(&h->modOsc, p[PAR_MOD_OSC_F1]);
	setCoarse(&h->modOsc2, p[PAR_MOD_OSC_F2]);
	h->fmModAmount1 = toFloat(p[PAR_MOD_OSC_GAIN1]);
	h->fmModAmount2 = toFloat(p[PAR_MOD_OSC_GAIN2]);

	setFilterFreq(&h->filter, p[PAR_FILTER_FREQ_6]);
	SVF_setReso(&h->filter, toFloat(p[PAR_RESO_6]));
	SVF_setDrive(&h->filter, toFloat(p[PAR_FILTER_DRIVE_6]));
	h->filterType = p[PAR_FILTER_TYPE_6] + 1;

	slopeEg2_setAttack(&h->oscVolEg, p[PAR_VELOA6], 0);
	slopeEg2_setSlope(&h->oscVolEg, p[PAR_VOL_SLOPE6]);
	h->decayClosed = slopeEg2_calcDecay(finiteDecay(p[PAR_VELOD6_CLOSED]));
	h->decayOpen = slopeEg2_calcDecay(finiteDecay(p[PAR_VELOD6_OPEN]));

	h->vol = toFloat(p[PAR_VOL6]);
	setDistortionShape(&h->distortion, toFloat(p[PAR_HAT_DISTORTION]));
	h->volumeMod = p[PAR_VOLUME_MOD_ON_OFF6] != 0;
	h->pwm = toFloat(p[PAR_PWM_6]);

	transient_setWaveform(&h->transGen, p[PAR_TRANS6_WAVE]);
	h->transGen.volume = toFloat(p[PAR_TRANS6_VOL]);
	h->transGen.pitch = transientPitch(p[PAR_TRANS6_FREQ]);

	instance->decimationRate = decimationRate(p[PAR_VOICE_DECIMATION6]);
}

/* ---- Public interface ------------------------------------------------------------------------ */

void delxr_lxr_voice_trigger(DelxrLxrVoice* voice, DelxrLxrEngine engine, const uint8_t* kit, uint8_t velocity) {
	Instance* instance = (Instance*)voice;
	memset(instance, 0, sizeof(*instance));
	instance->engine = engine;
	if (velocity == 0) {
		velocity = 1;
	}

	switch (engine) {
	case DELXR_LXR_DRUM1:
	case DELXR_LXR_DRUM2:
	case DELXR_LXR_DRUM3:
		initDrumVoice(&instance->v.drum);
		applyDrum(instance, kit, engine - DELXR_LXR_DRUM1);
		Drum_trigger(&instance->v.drum, velocity, SEQ_DEFAULT_NOTE);
		/* LXR voices live forever, so their parameter smoothers have long settled; this one is new. */
		setOnePoleValue(&instance->v.drum.MixerPageFilter[0], instance->v.drum.vol);
		setOnePoleValue(&instance->v.drum.distortion.DriveParamFilter, instance->v.drum.distortion.shape);
		break;
	case DELXR_LXR_SNARE:
		Snare_init(&instance->v.snare);
		applySnare(instance, kit);
		Snare_trigger(&instance->v.snare, velocity, SEQ_DEFAULT_NOTE);
		break;
	case DELXR_LXR_CYMBAL:
		Cymbal_init(&instance->v.cymbal);
		applyCymbal(instance, kit);
		Cymbal_trigger(&instance->v.cymbal, velocity, SEQ_DEFAULT_NOTE);
		break;
	case DELXR_LXR_CLOSED_HAT:
	case DELXR_LXR_OPEN_HAT:
	default:
		instance->engine = engine == DELXR_LXR_OPEN_HAT ? DELXR_LXR_OPEN_HAT : DELXR_LXR_CLOSED_HAT;
		HiHat_init(&instance->v.hat);
		applyHat(instance, kit);
		HiHat_trigger(&instance->v.hat, velocity, instance->engine == DELXR_LXR_OPEN_HAT, SEQ_DEFAULT_NOTE);
		break;
	}

	instance->decimationCount = 1.f; /* take the first sample immediately */
	instance->blockPos = OUTPUT_DMA_SIZE;
}

/* Transposes an oscillator by pitchRatio, keeping it below 20 kHz so its phase increment
 * cannot overflow. */
static void transpose(OscInfo* osc, float pitchRatio) {
	float ratio = pitchRatio;
	if (osc->freq > 0.f && osc->freq * ratio > 20000.f) {
		ratio = 20000.f / osc->freq;
	}
	osc->modNodeValue = ratio;
}

/* Renders the next 64-sample block. Returns 1 if the block was rendered at zero gain because the
 * amplitude envelope has ended. */
static int renderBlock(Instance* instance, float pitchRatio) {
	int ended = 0;
	switch (instance->engine) {
	case DELXR_LXR_DRUM1:
	case DELXR_LXR_DRUM2:
	case DELXR_LXR_DRUM3: {
		DrumVoice* d = &instance->v.drum;
		transpose(&d->osc, pitchRatio);
		transpose(&d->modOsc, pitchRatio);
		calcDrumVoiceAsync(d);
		calcDrumVoiceSyncBlock(d, instance->block, OUTPUT_DMA_SIZE);
		ended = d->oscVolEg.state == EG_D && d->oscVolEg.value <= 0. && d->ampFilterInput == 0.
		        && d->lastGain == 0.;
		break;
	}
	case DELXR_LXR_SNARE: {
		SnareVoice* s = &instance->v.snare;
		transpose(&s->osc, pitchRatio); /* the LXR does not transpose the snare's noise either */
		Snare_calcAsync(s);
		Snare_calcSyncBlock(s, instance->block, OUTPUT_DMA_SIZE);
		ended = s->oscVolEg.state == EG_D && s->oscVolEg.value <= 0. && s->egValueOscVol == 0.f;
		break;
	}
	case DELXR_LXR_CYMBAL: {
		CymbalVoice* c = &instance->v.cymbal;
		transpose(&c->osc, pitchRatio);
		transpose(&c->modOsc, pitchRatio);
		transpose(&c->modOsc2, pitchRatio);
		Cymbal_calcAsync(c);
		Cymbal_calcSyncBlock(c, instance->block, OUTPUT_DMA_SIZE);
		ended = c->oscVolEg.state == EG_D && c->oscVolEg.value <= 0. && c->egValueOscVol == 0.f;
		break;
	}
	default: {
		HiHatVoice* h = &instance->v.hat;
		transpose(&h->osc, pitchRatio);
		transpose(&h->modOsc, pitchRatio);
		transpose(&h->modOsc2, pitchRatio);
		HiHat_calcAsync(h);
		HiHat_calcSyncBlock(h, instance->block, OUTPUT_DMA_SIZE);
		ended = h->oscVolEg.state == EG_D && h->oscVolEg.value <= 0. && h->egValueOscVol == 0.f;
		break;
	}
	}

	/* mixer_decimateBlock(): sample-and-hold at a fraction of the sample rate */
	for (int i = 0; i < OUTPUT_DMA_SIZE; i++) {
		instance->decimationCount += instance->decimationRate;
		if (instance->decimationCount >= 1.f) {
			instance->decimationCount -= 1.f;
			instance->decimationSample = instance->block[i];
		}
		instance->block[i] = instance->decimationSample;
	}
	return ended;
}

int delxr_lxr_voice_render(DelxrLxrVoice* voice, float pitchRatio, int16_t* out, int32_t numSamples) {
	Instance* instance = (Instance*)voice;
	if (!(pitchRatio > 0.f)) {
		pitchRatio = 1.f;
	}
	if (pitchRatio < 1.f / 16.f) {
		pitchRatio = 1.f / 16.f;
	}
	if (pitchRatio > 8.f) {
		pitchRatio = 8.f;
	}

	for (int32_t i = 0; i < numSamples; i++) {
		if (instance->finished) {
			out[i] = 0;
			continue;
		}
		if (instance->blockPos >= OUTPUT_DMA_SIZE) {
			if (renderBlock(instance, pitchRatio)) {
				instance->finished = 1;
			}
			instance->blockPos = 0;
		}
		out[i] = instance->block[instance->blockPos++];
	}
	return !instance->finished;
}
