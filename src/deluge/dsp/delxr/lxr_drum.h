/*
 * Delxr's C++ face of the LXR drum voice (see src/deluge/dsp/lxr/README.md).
 *
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#pragma once

#include "dsp/lxr/delxr_lxr_drum.h"

namespace delxr {

/// One sounding LXR drum voice. About 640 bytes, so voices borrow one from a pool while they sound.
struct LxrDrumVoice {
	DelxrLxrDrum drum;
	bool needsTrigger = true;
};

/// Returns nullptr if memory is exhausted.
LxrDrumVoice* acquireLxrDrumVoice();
void releaseLxrDrumVoice(LxrDrumVoice* voice);

} // namespace delxr
