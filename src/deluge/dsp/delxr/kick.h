/*
 * Delxr kick: a sine whose pitch falls quickly to the played note while its level decays.
 * Written from scratch for Delxr. Not derived from LXR or Elixir code.
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

/// The four controls of the kick, each from 0 to 1.
struct KickControls {
	float decay;     // how long the kick rings: 20 ms to 1.2 s
	float sweep;     // how far above the note the pitch starts: none to 16 times
	float sweepTime; // how fast the pitch falls: 5 ms to 200 ms
	float drive;     // soft saturation: clean to 8 times gain
};

/// The kick engine type: the algorithm. Stateless; all state lives in KickVoice.
struct Kick {
	static constexpr float kSampleRate = 44100.0f;
	static constexpr int32_t kSilence = 2147484;    // -60 dB: the voice is finished below this
	static constexpr int32_t kOctavesBelowNote = 2; // a kit row plays middle C, so sit two octaves under it

	/// Per-sample multiplier, as q31, for an envelope that falls to 1/e in the given time.
	static int32_t decayPerSample(float seconds) {
		return INT32_MAX - static_cast<int32_t>(2147483648.0f / (seconds * kSampleRate));
	}

	/// Adds numSamples of kick, at up to full scale q31, into out. Returns false once the kick has died away.
	[[gnu::hot]] static bool render(KickVoice& voice, const KickControls& controls, int32_t* out, int32_t numSamples,
	                                uint32_t notePhaseIncrement) {
		const int32_t ampDecay = decayPerSample(0.02f + 1.2f * controls.decay * controls.decay);
		const int32_t pitchDecay = decayPerSample(0.005f + 0.2f * controls.sweepTime * controls.sweepTime);

		uint32_t base = notePhaseIncrement >> kOctavesBelowNote;
		if (base > (1u << 27)) {
			base = 1u << 27;
		}
		// The sweep is capped a little under half the sample rate, so the pitch can never alias.
		float sweepFloat = static_cast<float>(base) * 16.0f * controls.sweep * controls.sweep;
		const int32_t sweep = static_cast<int32_t>(sweepFloat < 1.0e9f ? sweepFloat : 1.0e9f);

		const bool driven = controls.drive > 0.001f;
		const int32_t gain = static_cast<int32_t>((1.0f + 7.0f * controls.drive) * 134217728.0f); // q27, 1 to 8

		for (int32_t i = 0; i < numSamples; i++) {
			voice.phase += base + (multiply_32x32_rshift32(sweep, voice.pitchEnv) << 1);
			int32_t sample = deluge::dsp::SineOsc::doFMNew(voice.phase, 0);
			sample = multiply_32x32_rshift32(sample, voice.ampEnv) << 1;

			if (driven) {
				// Boost, clip to full scale, then round the corners: y = 1.5x - 0.5x^3
				int32_t x = signed_saturate<32 - 5>(multiply_32x32_rshift32(sample, gain)) << 5;
				int32_t xSquared = multiply_32x32_rshift32(x, x) << 1;
				sample = signed_saturate<32 - 2>(multiply_32x32_rshift32(x, 1610612736 - (xSquared >> 2))) << 2;
			}
			out[i] = sample;

			voice.pitchEnv = multiply_32x32_rshift32(voice.pitchEnv, pitchDecay) << 1;
			voice.ampEnv = multiply_32x32_rshift32(voice.ampEnv, ampDecay) << 1;
		}
		return voice.ampEnv > kSilence;
	}
};

} // namespace delxr
