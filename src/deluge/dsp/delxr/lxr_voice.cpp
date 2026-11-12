/*
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#include "dsp/delxr/lxr_voice.h"
#include "memory/fast_allocator.h"
#include "memory/memory_allocator_interface.h"
#include "memory/object_pool.h"
#include <cstring>
#include <new>

namespace delxr {

LxrPatch::LxrPatch() {
	std::memcpy(kit, delxr_lxr_init_kit(), sizeof(kit));
}

LxrPatch* newLxrPatch() {
	void* memory = allocLowSpeed(sizeof(LxrPatch));
	if (memory == nullptr) {
		return nullptr;
	}
	return new (memory) LxrPatch;
}

void deleteLxrPatch(LxrPatch* patch) {
	if (patch != nullptr) {
		patch->~LxrPatch();
		delugeDealloc(patch);
	}
}

namespace {
constexpr char const* kEngineNames[] = {"drum1", "drum2", "drum3", "snare", "cymbal", "closedHat", "openHat"};
constexpr int kNumEngines = sizeof(kEngineNames) / sizeof(kEngineNames[0]);
} // namespace

char const* lxrEngineToString(DelxrLxrEngine engine) {
	int index = static_cast<int>(engine);
	return (index >= 0 && index < kNumEngines) ? kEngineNames[index] : kEngineNames[0];
}

DelxrLxrEngine stringToLxrEngine(char const* string) {
	for (int i = 0; i < kNumEngines; i++) {
		if (!std::strcmp(string, kEngineNames[i])) {
			return static_cast<DelxrLxrEngine>(i);
		}
	}
	return DELXR_LXR_DRUM1;
}

using LxrVoicePool = deluge::memory::ObjectPool<LxrVoice, deluge::memory::fast_allocator>;

LxrVoice* acquireLxrVoice() {
	try {
		return LxrVoicePool::get().acquire().release();
	} catch (deluge::exception e) {
		return nullptr;
	}
}

void releaseLxrVoice(LxrVoice* voice) {
	LxrVoicePool::recycle(voice);
}

} // namespace delxr
