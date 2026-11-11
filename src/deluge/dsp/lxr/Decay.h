/*
 * Decay.h
 *
 *  Created on: 16.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md.
 *
 * Modified for Delxr on 2026-11-11: include paths changed for building inside the
 * Deluge firmware. NOTICE.md is alongside this file; see README.md there for provenance.
 */


#ifndef DECAY_H_
#define DECAY_H_

#include <stdint.h>


typedef struct Decay_EG_Struct
{
	float decay;
	float slope;
	float value;
} DecayEg;


void DecayEg_init(DecayEg* eg);
void DecayEg_trigger(DecayEg* eg);
void DecayEg_setDecay(DecayEg* eg, uint8_t data2);
float DecayEg_calc(DecayEg* eg);
void DecayEg_setSlope(DecayEg* eg, uint8_t data2);

#endif /* DECAY_H_ */
