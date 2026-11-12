/*
 * Delxr's C++ face of the LXR voices (see src/deluge/dsp/lxr/README.md).
 *
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#pragma once

#include "dsp/lxr/delxr_lxr_voice.h"
#include <cstdint>

namespace delxr {

/// The sound settings of one LXR oscillator: which LXR voice it plays, and that voice's
/// parameters in the LXR kit's own byte layout. One per oscillator that uses the LXR type.
struct LxrPatch {
	DelxrLxrEngine engine = DELXR_LXR_DRUM1;
	uint8_t kit[DELXR_LXR_KIT_BYTES];
	uint8_t lfoTarget = 0; ///< LXR parameter number the voice's LFO modulates, 0 = none

	LxrPatch(); ///< starts as the LXR's built-in init kit
};

/// Returns nullptr if memory is exhausted.
LxrPatch* newLxrPatch();
void deleteLxrPatch(LxrPatch* patch);

/// File name of each engine, as saved in kits and songs.
char const* lxrEngineToString(DelxrLxrEngine engine);
DelxrLxrEngine stringToLxrEngine(char const* string);

/// One sounding LXR voice. Voices borrow one from a pool while they sound.
struct LxrVoice {
	DelxrLxrVoice voice;
	bool needsTrigger = true;
	uint8_t velocity = 127;
};

/// Returns nullptr if memory is exhausted.
LxrVoice* acquireLxrVoice();
void releaseLxrVoice(LxrVoice* voice);

} // namespace delxr
