/*
 * HiHat.c
 *
 *  Created on: 18.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md.
 *
 * Modified for Delxr on 2026-11-12. The LXR firmware has one global hihat voice; here every function
 * takes a pointer to one voice, so any number of independent voices can exist. Removed, because Delxr
 * supplies them per voice elsewhere or the Deluge has its own: the LFO member, panning, the velocity
 * modulation nodes and the panel fader. The saturating add of the transient is written per sample
 * instead of on packed pairs (same result). The signal path is unchanged. NOTICE.md is alongside this
 * file; see README.md there for provenance.
 */



#include "HiHat.h"
#include "BufferTools.h"
#include <math.h>
#include <string.h>


//---------------------------------------------------

void HiHat_init(HiHatVoice* voice)
{
	memset(voice, 0, sizeof(*voice));

	SnapEg_init(&voice->snapEg);
	voice->vol = 0.8f;

	//voice->panModifier = 1.f;

	transient_init(&voice->transGen);

	voice->fmModAmount1 = 0.5f;
	voice->fmModAmount2 = 0.5f;

	setDistortionShape(&voice->distortion, 0.f);

	voice->modOsc.freq = 440;
	voice->modOsc.waveform = SINE;
	voice->modOsc.fmMod = 0;
	voice->modOsc.midiFreq = 70<<8 | 64 ;
	voice->modOsc.pitchMod = 1.0f;
	voice->modOsc.modNodeValue = 1;

	voice->modOsc2.freq = 440;
	voice->modOsc2.waveform = NOISE;//SINE;
	voice->modOsc2.fmMod = 0;
	voice->modOsc2.midiFreq = 70<<8 | 64;
	voice->modOsc2.pitchMod = 1.0f;
	voice->modOsc2.modNodeValue = 1;

	voice->osc.freq = 440;
	voice->osc.waveform = 1;
	voice->osc.fmMod = 1;
	voice->osc.midiFreq = 70<<8 | 64;
	voice->osc.pitchMod = 1.0f;
	voice->osc.modNodeValue = 1;

	voice->volumeMod = 1;

	slopeEg2_init(&voice->oscVolEg);

	SVF_init(&voice->filter);
}

//---------------------------------------------------
void HiHat_trigger(HiHatVoice* voice, uint8_t vel, uint8_t isOpen, const uint8_t note)
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

	voice->modOsc.phase = 0;
	voice->modOsc2.phase = 0;

	osc_setBaseNote(&voice->osc,note);
	osc_setBaseNote(&voice->modOsc,note);
	osc_setBaseNote(&voice->modOsc2,note);

	voice->isOpen = isOpen;
	voice->oscVolEg.decay = isOpen?voice->decayOpen:voice->decayClosed;

	slopeEg2_trigger(&voice->oscVolEg);
	voice->velo = vel/127.f;
	transient_trigger(&voice->transGen);

	SnapEg_trigger(&voice->snapEg);
}
//---------------------------------------------------
IN_ITCM_RAM void HiHat_calcAsync(HiHatVoice* voice)
{
	//calc the osc  vol eg
	voice->egValueOscVol = slopeEg2_calc(&voice->oscVolEg);

	//turn off trigger signal if trigger gate mode is on and volume == 0
	/*
	if(trigger_isGateModeOn())
	{
		if(!voice->egValueOscVol)
		{
			if(voice->isOpen)
			{
				//trigger_triggerVoice(TRIGGER_7, TRIGGER_OFF);
				voiceControl_noteOff(TRIGGER_7);
			} else {
				//trigger_triggerVoice(TRIGGER_6, TRIGGER_OFF);
				voiceControl_noteOff(TRIGGER_6);
			}
		}
	}
	*/

	//calc snap EG if transient sample 0 is activated
	if(voice->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&voice->snapEg, voice->transGen.pitch);
		voice->osc.pitchMod = 1 + snapVal*voice->transGen.volume;
	}

	osc_setFreq(&voice->osc);
	osc_setFreq(&voice->modOsc);
	osc_setFreq(&voice->modOsc2);


}
//---------------------------------------------------
void HiHat_calcSyncBlock(HiHatVoice* voice, int16_t* buf, const uint8_t size)
{
	//2 buffers for the mod oscs
	int16_t mod1[size],mod2[size];
	//calc next mod osc samples, scaled with mod amount
	calcNextOscSampleBlock(&voice->modOsc,mod1,size, voice->fmModAmount1, voice->pwm);
	calcNextOscSampleBlock(&voice->modOsc2,mod2,size,  voice->fmModAmount2, voice->pwm);

	//combine both mod oscs to 1 modulation signal
	bufferTool_addBuffersSaturating(mod1,mod2,size);

	calcNextOscSampleFmBlock(&voice->osc,mod1,buf,size,0.5f, voice->pwm) ;

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
	//calc transient sample
	transient_calcBlock(&voice->transGen,mod1,size);

	uint8_t j;
	if(voice->volumeMod)
	{
		voice->combinedVolume = voice->vol * voice->velo * voice->egValueOscVol; ;
		//const float vol =

#if 1
			//add transient to buffer
			bufferTool_addBuffersSaturating(buf,mod1,size);

			for(j=0;j<size;j++)
			{
				//add volume
				buf[j] *=  voice->combinedVolume;
			}
#else
		for(j=0;j<size;j++)
		{
			//add filter to buffer
			buf[j] = __QADD16(buf[j],mod1[j]);
			buf[j] *= vol;
		}
#endif
	}
	else
	{
		voice->combinedVolume = voice->vol * voice->egValueOscVol;
		//const float vol = voice->combinedVolume ;
#if 1
			//add transient to buffer
			bufferTool_addBuffersSaturating(buf,mod1,size);

			for(j=0;j<size;j++)
			{
				//add volume
				buf[j] *=  voice->combinedVolume;
			}
#else
		for(j=0;j<size;j++)
		{
			//add filter to buffer
			buf[j] = __QADD16(buf[j],mod1[j]);

			buf[j] *= vol;
		}
#endif
	}

	calcDistBlock(&voice->distortion,buf,size);
}
//---------------------------------------------------

