/*
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#pragma once

#include "dsp/delxr/oscillator_type.h"
#include "dsp/lxr/delxr_lxr_metal.h"
#include "dsp/lxr/delxr_lxr_snare.h"

namespace delxr {

// DSP state belongs to a single sounding unison source. Closed/open hats and cymbals share a pool,
// never a sounding instance, so kit rows, polyphony and unison remain independent.
struct LxrSnareVoice {
	DelxrLxrSnare snare;
	bool needsTrigger = true;
};

struct LxrMetalVoice {
	DelxrLxrMetal metal;
	bool needsTrigger = true;
};

// Returns nullptr on allocation failure, allowing the host voice to stop cleanly.
LxrSnareVoice* acquireLxrSnareVoice();
void releaseLxrSnareVoice(LxrSnareVoice* voice);
LxrMetalVoice* acquireLxrMetalVoice();
void releaseLxrMetalVoice(LxrMetalVoice* voice);

} // namespace delxr
