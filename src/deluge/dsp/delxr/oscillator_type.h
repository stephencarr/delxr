/*
 * This file is part of Delxr, a fork of the Deluge community firmware. Licensed under the GNU
 * General Public License version 3, with the additional terms in src/deluge/dsp/lxr/NOTICE.md.
 */

#pragma once

#include "definitions_cxx.hpp"

namespace delxr {

constexpr bool isLxrMetal(OscType type) {
	return type == OscType::DELXR_LXR_CYMBAL || type == OscType::DELXR_LXR_CLOSED_HAT
	       || type == OscType::DELXR_LXR_OPEN_HAT;
}

constexpr bool isLxr(OscType type) {
	return type == OscType::DELXR_LXR_DRUM || type == OscType::DELXR_LXR_SNARE || isLxrMetal(type);
}

constexpr bool isEngine(OscType type) {
	return type == OscType::DELXR_KICK || isLxr(type);
}

} // namespace delxr
