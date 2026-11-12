/*
 * Snare.c
 *
 *  Created on: 17.04.2012
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



#include "Snare.h"
#include "BufferTools.h"
#include <math.h>
#include <string.h>
//instance of the snare voice

//---------------------------------------------------
void Snare_init(SnareVoice* voice)
{
	memset(voice, 0, sizeof(*voice));

	SnapEg_init(&voice->snapEg);
	voice->vol = 0.8f;

	//voice->panModifier = 1.f;

	voice->noiseOsc.freq = 440;
	voice->noiseOsc.waveform = 1;
	voice->noiseOsc.fmMod = 0;
	voice->noiseOsc.midiFreq = 70<<8 | 64;
	voice->noiseOsc.pitchMod = 1.0f;
	voice->noiseOsc.modNodeValue = 1;

	voice->osc.freq = 440;
	voice->osc.waveform = 1;
	voice->osc.fmMod = 0;
	voice->osc.midiFreq = 70<<8 | 64;
	voice->osc.modNodeValue = 1;

	setDistortionShape(&voice->distortion, 0.f);

	voice->volumeMod = 1;

	transient_init(&voice->transGen);

	DecayEg_init(&voice->oscPitchEg);
	voice->egPitchModAmount = 0.5f;

	slopeEg2_init(&voice->oscVolEg);



	SVF_init(&voice->filter);
}
//---------------------------------------------------
void Snare_trigger(SnareVoice* voice, const uint8_t vel, const uint8_t note)
{

	float offset = 1;
	if(voice->transGen.waveform==1) //offset mode
	{
		offset -= voice->transGen.volume;
	}
	if(voice->osc.waveform == SINE)
		voice->osc.phase = (0x3ff<<20)*offset;//voiceArray[voiceNr].osc.startPhase ;
	else if(voice->osc.waveform > SINE && voice->osc.waveform <= REC)
		voice->osc.phase = (0xff<<22)*offset;
	else
		voice->osc.phase = 0;

	DecayEg_trigger(&voice->oscPitchEg);
	slopeEg2_trigger(&voice->oscVolEg);
	voice->velo = vel/127.f;

	osc_setBaseNote(&voice->osc,note);
	//TODO noise muss mit transponiert werden

	transient_trigger(&voice->transGen);

	SnapEg_trigger(&voice->snapEg);
}
//---------------------------------------------------
IN_ITCM_RAM void Snare_calcAsync(SnareVoice* voice)
{
	//add modulation eg to osc freq (1 = no change. a+eg = original freq + modulation
	const float egPitchVal = DecayEg_calc(&voice->oscPitchEg);
	const float pitchEgValue = egPitchVal*voice->egPitchModAmount;
	voice->osc.pitchMod = 1+pitchEgValue;

	//calc the osc  vol eg
	voice->egValueOscVol = slopeEg2_calc(&voice->oscVolEg);

	//turn off trigger signal if trigger gate mode is on and volume == 0
	/*
	if(trigger_isGateModeOn())
	{
		if(!voice->egValueOscVol) {
			//trigger_triggerVoice(TRIGGER_4, TRIGGER_OFF);
			//voiceControl_noteOff(TRIGGER_4);
		}
	}
	*/

	//calc snap EG if transient sample 0 is activated
	if(voice->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&voice->snapEg, voice->transGen.pitch);
		voice->osc.pitchMod += snapVal*voice->transGen.volume;;
	}

	osc_setFreq(&voice->osc);
	osc_setFreq(&voice->noiseOsc);


}
//---------------------------------------------------
void Snare_calcSyncBlock(SnareVoice* voice, int16_t* buf, const uint8_t size)
{
	int16_t transBuf[size];
	int16_t oscBuf[size];

	calcNoiseBlock(&voice->noiseOsc,buf,size,0.9f);
#if USE_REVERB
	if(fxBus.fxType != FX_REVERB)
	{
#endif
		SVF_calcBlockZDF(&voice->filter,voice->filterType,buf,size);
#if USE_REVERB
	}
	else
	{
		SVF_calcBlockCheapZDF(&voice->filter,voice->filterType,buf,size);
	}
#endif




	//calc next osc sample
	calcNextOscSampleBlock(&voice->osc,oscBuf,size,(1.f-voice->mix), voice->pwm);
	// apply filter to synthesized sound as well here if desired, or combine code for more efficiency
	//SVF_calcBlockZDF(&voice->filter,voice->filterType,transBuf,size);


	//calc transient sample
	transient_calcBlock(&voice->transGen,transBuf,size);


	uint8_t j;
	if(voice->volumeMod)
	{
		voice->combinedVolume = voice->vol * voice->velo * voice->egValueOscVol ;
		//const float vol = ;

		for(j=0;j<size;j++)
		{

#if 1
			//float fmaf(float a, float b, float c);
			//result = a × b + c
			float tmp = fmaf(buf[j], voice->mix, oscBuf[j]);
			//add transient sample
			//tmp	= __QADD16(transBuf[j],(int16_t)tmp);
			tmp	+= transBuf[j];
			buf[j] =   voice->combinedVolume  * tmp;

#else

			//add filter to buffer
			buf[j] *= voice->mix;
			buf[j] = (__QADD16(buf[j],transBuf[j])); //transBuf here is mod osc

			buf[j] *=  vol;
#endif
		}
	}
	else
	{
		voice->combinedVolume = voice->vol * voice->egValueOscVol;
		//const float vol = voice->combinedVolume * ;

		for(j=0;j<size;j++)
		{

#if 1
			float tmp = fmaf(buf[j], voice->mix, oscBuf[j]);
			//add transient sample
			//tmp	= __QADD16(transBuf[j],(int16_t)tmp);
			tmp	+= transBuf[j];
			buf[j] =  voice->combinedVolume * tmp;
#else
			//add filter to buffer
			buf[j] *= voice->mix;
			buf[j] = (__QADD16(buf[j],transBuf[j]));
			buf[j] *=  vol;
#endif
		}
	}

	calcDistBlock(&voice->distortion,buf,size);
}
//------------------------------------------------------------------------
