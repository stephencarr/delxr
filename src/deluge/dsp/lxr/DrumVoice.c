/*
 * DrumVoice.c
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

#include "DrumVoice.h"
#include "BufferTools.h"
#include "Oscillator.h"
#include "math.h"
#include <string.h>

//---------------------------------------------------
void initDrumVoice(DrumVoice* voice)
{
	memset(voice, 0, sizeof(*voice));

	for(uint8_t j=0;j<2;j++)
	{
		initOnePole(&voice->MixerPageFilter[j]);
		setOnePoleCoef(&voice->MixerPageFilter[j],0.01);
	}
	initOnePole(&voice->distortion.DriveParamFilter);
	setOnePoleCoef(&voice->distortion.DriveParamFilter,0.01);

	SnapEg_init(&voice->snapEg);
	voice->vol = 0.8f;
	voice->fmModAmount = 0.5f;
	transient_init(&voice->transGen);
	SVF_init(&voice->filter);
	voice->filterType = 0x01;

	voice->modOsc.freq = 440;
	voice->modOsc.waveform = 1;
	voice->modOsc.fmMod = 0;
	voice->modOsc.midiFreq = 70<<8 | 64;
	voice->modOsc.pitchMod = 1.0f;
	voice->modOsc.modNodeValue = 1;

	voice->volumeMod = 1;

	voice->osc.freq = 440;
	voice->osc.modNodeValue = 1;
	voice->osc.waveform = TRI;
	voice->osc.fmMod = 0;
	voice->osc.midiFreq = 70<<8 | 64;

	DecayEg_init(&voice->oscPitchEg);
	voice->egPitchModAmount = 0.5f;

	slopeEg2_init(&voice->oscVolEg);
	setDistortionShape(&voice->distortion, 0.f);

	voice->mixOscs = 1;
}
//---------------------------------------------------
void Drum_trigger(DrumVoice* voice, const uint8_t vol, const uint8_t note)
{
	{
		float offset = 1;
		if(voice->transGen.waveform==1) //offset mode
		{
			offset -= voice->transGen.volume;
		}
		if(voice->osc.waveform == SINE)
			voice->osc.phase = 1024 + ( (0x3ff<<20) - 1024)*offset;
		else if(voice->osc.waveform > SINE && voice->osc.waveform <= REC)
			voice->osc.phase = (0xff<<22)*offset;
		else
			voice->osc.phase = 0;

		//reset mod osc phase
		if(voice->modOsc.waveform == SINE)
			voice->modOsc.phase = 1024 + ( (0x3ff<<20) - 1024);
		else if(voice->modOsc.waveform > SINE && voice->modOsc.waveform <= REC)
			voice->modOsc.phase = (0xff<<22);
		else
			voice->modOsc.phase = 0;
	}

	osc_setBaseNote(&voice->osc,note);
	osc_setBaseNote(&voice->modOsc,note);

	DecayEg_trigger(&voice->oscPitchEg);
	slopeEg2_trigger(&voice->oscVolEg);
	voice->velo = vol/127.f;

	transient_trigger(&voice->transGen);

	SnapEg_trigger(&voice->snapEg);

	//reset filter coeffs to prevent wrong transient
	SVF_reset(&voice->filter);
}
//---------------------------------------------------
void calcDrumVoiceAsync(DrumVoice* voice)
{
	//add modulation eg to osc freq (1 = no change. a+eg = original freq + modulation
	const float egPitchVal = DecayEg_calc(&voice->oscPitchEg);
	const float pitchEgValue = egPitchVal*voice->egPitchModAmount;
	voice->osc.pitchMod = 1+pitchEgValue;

	//calc snap EG if transient sample 0 is activated
	if(voice->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&voice->snapEg, voice->transGen.pitch);
		voice->osc.pitchMod += snapVal*voice->transGen.volume;
	}

	// fm amount with pitch eg
	voice->osc.fmMod = voice->fmModAmount * egPitchVal;

	//calc the osc + noise vol eg
	//check if in attack phase
	if( (voice->oscVolEg.attack == 1 ) && ((voice->oscVolEg.state == EG_A) || (voice->oscVolEg.state == EG_REPEAT)) )
	{
		//if attack is set to 0 -> no interpolation
		voice->ampFilterInput = slopeEg2_calc(&voice->oscVolEg);
		voice->lastGain = voice->ampFilterInput;
	}
	else
	{
		voice->lastGain = voice->ampFilterInput;
		voice->ampFilterInput = slopeEg2_calc(&voice->oscVolEg);
	}

	//update osc phaseInc
	osc_setFreq(&voice->osc);
	osc_setFreq(&voice->modOsc);
}

//---------------------------------------------------
void calcDrumVoiceSyncBlock(DrumVoice* voice, int16_t* buf, const uint8_t size)
{
	int16_t modBuf[OUTPUT_DMA_SIZE]; // Delxr wrappers always render native 64-sample blocks

	//calc next mod osc sampleBlock - int16_t processing
	calcNextOscSampleBlock(&voice->modOsc,modBuf,size,voice->fmModAmount, voice->pwm);

	if(voice->mixOscs)
	{
		//calc main osc buffer - int16_t processing
		calcNextOscSampleBlock(&voice->osc,buf,size, (1.f-voice->fmModAmount), voice->pwm);
		//add mod buffer to main osc buffer - int16_t processing
		bufferTool_addBuffersSaturating(buf,modBuf,size);
	}
	else
	{
		calcNextOscSampleFmBlock(&voice->osc,modBuf,buf,size,1.0f, voice->pwm);
	}

	//calc transient sample - int16_t processing
	transient_calcBlock(&voice->transGen,modBuf,size);

	//Mix with transient buffer - int16_t processing
	bufferTool_addBuffersSaturating(buf,modBuf,size);

	//calc filter block - float processing
	SVF_calcBlockZDF(&voice->filter,voice->filterType,buf,size);

	//attentuate main OSCs by amp EG
	bufferTool_addGainInterpolated(buf,voice->ampFilterInput, voice->lastGain, size);

	//MIDI velocity - float processing
	if(voice->volumeMod)
	{
		bufferTool_addGain(buf,voice->velo,size);
	}
	//distortion - float processing
	calcDistBlockFiltered(&voice->distortion,buf,size);

	//channel volume
	for(int i=0; i< size; i++)
	{
		voice->combinedVolume = calcOnePole( &voice->MixerPageFilter[0], voice->vol);
		buf[i] *= voice->combinedVolume;
	}
}
//---------------------------------------------------
