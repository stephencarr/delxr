/*
 * CymbalVoice.c
 *
 *  Created on: 16.06.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 *
 * Modified for Delxr on 2026-11-11: global voice replaced by an instance pointer;
 * hardware, LFO, velocity, pan and fader services removed (provided by Deluge).
 * Signal order and native block-rate envelope retained. Initialization clears all
 * state; the final negative envelope overshoot is clamped to silence. See README.md.
 */

#include "CymbalVoice.h"
#include "BufferTools.h"
#include <string.h>

void Cymbal_init(CymbalVoice* voice) {
    memset(voice, 0, sizeof(*voice));
    SnapEg_init(&voice->snapEg);
    transient_init(&voice->transGen);
    slopeEg2_init(&voice->oscVolEg);
    SVF_init(&voice->filter);
    voice->filterType = FILTER_HP;
    voice->vol = 0.8f;
    voice->fmModAmount1 = voice->fmModAmount2 = 0.5f;
    setDistortionShape(&voice->distortion, 0.f);
    OscInfo* oscillators[] = {&voice->osc, &voice->modOsc, &voice->modOsc2};
    for (unsigned i = 0; i < 3; ++i) {
        oscillators[i]->freq = 440.f;
        oscillators[i]->midiFreq = (70 << 8) | 64;
        oscillators[i]->pitchMod = 1.f;
        oscillators[i]->modNodeValue = 1.f;
    }
    voice->osc.waveform = TRI;
    voice->osc.fmMod = 1.f;
    voice->modOsc.waveform = SINE;
    voice->modOsc2.waveform = NOISE;
}

void Cymbal_trigger(CymbalVoice* voice, uint8_t note) {
    float offset = 1.f;
    if (voice->transGen.waveform == 1) offset -= voice->transGen.volume;
    if (voice->osc.waveform == SINE) voice->osc.phase = (0x3ffu << 20) * offset;
    else if (voice->osc.waveform <= REC) voice->osc.phase = (0xffu << 22) * offset;
    else voice->osc.phase = 0;
    voice->modOsc.phase = voice->modOsc2.phase = 0;
    osc_setBaseNote(&voice->osc, note);
    osc_setBaseNote(&voice->modOsc, note);
    osc_setBaseNote(&voice->modOsc2, note);
    slopeEg2_trigger(&voice->oscVolEg);
    transient_trigger(&voice->transGen);
    SnapEg_trigger(&voice->snapEg);
}

void Cymbal_calcAsync(CymbalVoice* voice) {
    const float envelope = slopeEg2_calc(&voice->oscVolEg);
    voice->egValueOscVol = envelope > 0.f ? envelope : 0.f;
    if (voice->transGen.waveform == 0) {
        const float snap = SnapEg_calc(&voice->snapEg, voice->transGen.pitch);
        voice->osc.pitchMod = 1.f + snap * voice->transGen.volume;
    }
    osc_setFreq(&voice->osc);
    osc_setFreq(&voice->modOsc);
    osc_setFreq(&voice->modOsc2);
}

void Cymbal_calcSyncBlock(CymbalVoice* voice, int16_t* buf, uint8_t size) {
    int16_t mod1[OUTPUT_DMA_SIZE], mod2[OUTPUT_DMA_SIZE];
    calcNextOscSampleBlock(&voice->modOsc, mod1, size, voice->fmModAmount1, voice->pwm);
    calcNextOscSampleBlock(&voice->modOsc2, mod2, size, voice->fmModAmount2, voice->pwm);
    bufferTool_addBuffersSaturating(mod1, mod2, size);
    calcNextOscSampleFmBlock(&voice->osc, mod1, buf, size, 1.f, voice->pwm);
    SVF_calcBlockZDF(&voice->filter, voice->filterType, buf, size);
    transient_calcBlock(&voice->transGen, mod1, size);
    bufferTool_addBuffersSaturating(buf, mod1, size);
    voice->combinedVolume = voice->vol * voice->egValueOscVol;
    bufferTool_addGain(buf, voice->combinedVolume, size);
    calcDistBlock(&voice->distortion, buf, size);
}
