/*
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#include "dsp/delxr/lxr_voices.h"
#include "memory/fast_allocator.h"
#include "memory/object_pool.h"

namespace delxr {

using LxrSnareVoicePool = deluge::memory::ObjectPool<LxrSnareVoice, deluge::memory::fast_allocator>;
using LxrMetalVoicePool = deluge::memory::ObjectPool<LxrMetalVoice, deluge::memory::fast_allocator>;

LxrSnareVoice* acquireLxrSnareVoice() {
	try {
		return LxrSnareVoicePool::get().acquire().release();
	} catch (deluge::exception e) {
		return nullptr;
	}
}

void releaseLxrSnareVoice(LxrSnareVoice* voice) {
	LxrSnareVoicePool::recycle(voice);
}

LxrMetalVoice* acquireLxrMetalVoice() {
	try {
		return LxrMetalVoicePool::get().acquire().release();
	} catch (deluge::exception e) {
		return nullptr;
	}
}

void releaseLxrMetalVoice(LxrMetalVoice* voice) {
	LxrMetalVoicePool::recycle(voice);
}

} // namespace delxr
