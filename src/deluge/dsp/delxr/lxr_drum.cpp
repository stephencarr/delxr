/*
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#include "dsp/delxr/lxr_drum.h"
#include "memory/fast_allocator.h"
#include "memory/object_pool.h"

namespace delxr {

using LxrDrumVoicePool = deluge::memory::ObjectPool<LxrDrumVoice, deluge::memory::fast_allocator>;

LxrDrumVoice* acquireLxrDrumVoice() {
	try {
		return LxrDrumVoicePool::get().acquire().release();
	} catch (deluge::exception e) {
		return nullptr;
	}
}

void releaseLxrDrumVoice(LxrDrumVoice* voice) {
	LxrDrumVoicePool::recycle(voice);
}

} // namespace delxr
