/*
 * CymbalVoice.h
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

#ifndef DELXR_CYMBALVOICE_H_
#define DELXR_CYMBALVOICE_H_

#include "Oscillator.h"
#include "ResonantFilter.h"
#include "SlopeEg2.h"
#include "transientGenerator.h"
#include "distortion.h"
#include "snapEg.h"

typedef struct {
    OscInfo osc, modOsc, modOsc2;
    float fmModAmount1, fmModAmount2;
    float vol;
    ResonantFilter filter;
    uint8_t filterType;
    TransientGenerator transGen;
    Distortion distortion;
    SlopeEg2 oscVolEg;
    float egValueOscVol;
    SnapEg snapEg;
    float combinedVolume;
    float pwm;
} CymbalVoice;

void Cymbal_init(CymbalVoice* voice);
void Cymbal_trigger(CymbalVoice* voice, uint8_t note);
void Cymbal_calcAsync(CymbalVoice* voice);
void Cymbal_calcSyncBlock(CymbalVoice* voice, int16_t* buf, uint8_t size);
#endif
