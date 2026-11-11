/*
 * Delxr kick: a sine whose pitch falls quickly to the played note while its level decays.
 * Written from scratch for Delxr Experiment 001. Not derived from LXR or Elixir code.
 *
 * This file is part of Delxr, a fork of the Deluge community firmware, and is licensed
 * under the GNU General Public License, version 3 or later.
 */

#pragma once

#include "dsp/oscillators/sine_osc.h"
#include "util/fixedpoint.h"
#include <cstdint>

namespace delxr {

/// One sounding kick. Each voice owns one of these; the engine below holds no state of its own.
struct KickVoice {
	uint32_t phase = 0;
	int32_t pitchEnv = 0; // q31, falls from full scale towards zero
	int32_t ampEnv = 0;   // q31, falls from full scale towards zero

	void trigger() {
		phase = 0;
		pitchEnv = INT32_MAX;
		ampEnv = INT32_MAX;
	}
};

/// The kick engine type: the algorithm and its fixed character.
struct Kick {
	// Per-sample decay multipliers at 44.1 kHz, as q31: exp(-1 / (seconds * 44100)).
	static constexpr int32_t kPitchDecay = 2145050239; // 20 ms time constant
	static constexpr int32_t kAmpDecay = 2146942652;   // 90 ms time constant
	static constexpr int32_t kSilence = 2147484;       // -60 dB: the voice is finished below this
	static constexpr uint32_t kSweepRatio = 5;         // pitch starts this many times above the note, on top of it
	static constexpr int32_t kOctavesBelowNote = 2;    // a kit row plays middle C, so sit two octaves under it

	/// Adds numSamples of kick, at full scale q31, into out. Returns false once the kick has died away.
	[[gnu::hot]] static bool render(KickVoice& voice, int32_t* out, int32_t numSamples, uint32_t notePhaseIncrement) {
		uint32_t base = notePhaseIncrement >> kOctavesBelowNote;
		if (base > (1u << 27)) {
			base = 1u << 27; // keeps the sweep below the top of the audio band
		}
		const int32_t sweep = base * kSweepRatio;

		for (int32_t i = 0; i < numSamples; i++) {
			voice.phase += base + (multiply_32x32_rshift32(sweep, voice.pitchEnv) << 1);
			int32_t sine = deluge::dsp::SineOsc::doFMNew(voice.phase, 0);
			out[i] = multiply_32x32_rshift32(sine, voice.ampEnv) << 1;

			voice.pitchEnv = multiply_32x32_rshift32(voice.pitchEnv, kPitchDecay) << 1;
			voice.ampEnv = multiply_32x32_rshift32(voice.ampEnv, kAmpDecay) << 1;
		}
		return voice.ampEnv > kSilence;
	}
};

} // namespace delxr
