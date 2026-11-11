/*
 * snapEg.h
 *
 *  Created on: 16.06.2013
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


#ifndef SNAPEG_H_
#define SNAPEG_H_

#include <stdint.h>

typedef struct Snap_EG_Struct
{
	float value;
} SnapEg;


void SnapEg_init(SnapEg* eg);
void SnapEg_trigger(SnapEg* eg);
float SnapEg_calc(SnapEg* eg, float time);



#endif /* SNAPEG_H_ */
