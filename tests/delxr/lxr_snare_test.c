/* SPDX-License-Identifier: GPL-3.0-only
 * Behavioral tests for the actual ported LXR snare; link its production DSP sources.
 */
#include "delxr_lxr_snare.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define AUDIO_SIZE 8192
static const DelxrLxrSnareControls defaults = {0.40f, 0.60f, 0.70f, 0.f};

static void audio(DelxrLxrSnareControls controls, float pitch, int16_t* out) {
	DelxrLxrSnare snare;
	delxr_lxr_snare_trigger(&snare, &controls);
	delxr_lxr_snare_render(&snare, &controls, pitch, out, AUDIO_SIZE);
}

static void patchAudio(DelxrLxrSnareControls controls, const DelxrLxrPatch* patch, int16_t* out) {
	DelxrLxrSnare snare;
	delxr_lxr_snare_trigger_patch(&snare, &controls, patch);
	delxr_lxr_snare_render_patch(&snare, &controls, 1.f, out, AUDIO_SIZE, patch);
}

static void nativePatchTests(void) {
	int16_t reference[AUDIO_SIZE], other[AUDIO_SIZE];
	DelxrLxrPatch patch;
	delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);

	/* The original API and explicit NULL patch have identical PCM and call boundaries. */
	audio(defaults, 1.f, reference);
	patchAudio(defaults, NULL, other);
	assert(!memcmp(reference, other, sizeof(reference)));

	/* Every exposed extra changes an applicable signal, including the PWM carrier. */
	for (int param = 0; param < DELXR_LXR_PATCH_PARAM_COUNT; ++param) {
		if (!delxr_lxr_patch_param_relevant(param, DELXR_LXR_FAMILY_SNARE)) continue;
		delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);
		if (param == DELXR_LXR_PULSE_WIDTH) patch.value[DELXR_LXR_CARRIER_WAVE] = 5;
		if (param == DELXR_LXR_TRANSIENT_TUNE) patch.value[DELXR_LXR_TRANSIENT_WAVE] = 9;
		patch.value[param] = param == DELXR_LXR_NOISE_TUNE ? 1 : 0;
		patchAudio(defaults, &patch, reference);
		patch.value[param] = delxr_lxr_patch_param_max(param);
		patchAudio(defaults, &patch, other);
		if (!memcmp(reference, other, sizeof(reference))) fprintf(stderr, "inaudible native param: %s\n", delxr_lxr_patch_param_name(param));
		assert(memcmp(reference, other, sizeof(reference)));
	}

	/* Native SNARE_NOISE_F spans an actual held clock at zero through 22 kHz.
	 * Isolate that branch: no tonal oscillator, transient, or filter coloration. */
	DelxrLxrSnareControls noise = defaults;
	noise.snappy = 1.f;
	delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);
	patch.value[DELXR_LXR_TRANSIENT_LEVEL] = 0;
	patch.value[DELXR_LXR_FILTER_TYPE] = 7;
	patch.value[DELXR_LXR_NOISE_TUNE] = 0;
	patchAudio(noise, &patch, reference);
	for (int i = 0; i < AUDIO_SIZE; ++i) assert(reference[i] == 0);
	patch.value[DELXR_LXR_NOISE_TUNE] = 1;
	patchAudio(noise, &patch, reference);
	patch.value[DELXR_LXR_NOISE_TUNE] = 127;
	patchAudio(noise, &patch, other);
	int slowCrossings = 0, fastCrossings = 0;
	for (int i = 1; i < AUDIO_SIZE; ++i) {
		slowCrossings += (reference[i] < 0) != (reference[i - 1] < 0);
		fastCrossings += (other[i] < 0) != (other[i - 1] < 0);
	}
	assert(slowCrossings > 0 && fastCrossings > 20 * slowCrossings);
	/* The native filter reaches zero cutoff, unlike the legacy 500-Hz minimum. */
	patch.value[DELXR_LXR_FILTER_TYPE] = 2;
	noise.tone = 0.f;
	patchAudio(noise, &patch, reference);
	for (int i = 0; i < AUDIO_SIZE; ++i) assert(reference[i] == 0);
	noise.tone = 1.f;
	patchAudio(noise, &patch, other);
	assert(memcmp(reference, other, sizeof(reference)));

	/* The snare has no FM bus: hidden FM controls cannot change this voice. */
	delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);
	patchAudio(defaults, &patch, reference);
	for (int param = 0; param < DELXR_LXR_PATCH_PARAM_COUNT; ++param)
		if (!delxr_lxr_patch_param_relevant(param, DELXR_LXR_FAMILY_SNARE)) patch.value[param] = 255;
	patchAudio(defaults, &patch, other);
	assert(!memcmp(reference, other, sizeof(reference)));

	/* Six actual native carrier algorithms produce distinct PCM (noise branch muted). */
	int16_t waveforms[6][AUDIO_SIZE];
	DelxrLxrSnareControls tonal = defaults;
	tonal.snappy = 0.f;
	delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);
	patch.value[DELXR_LXR_TRANSIENT_LEVEL] = 0;
	for (int wave = 0; wave < 6; ++wave) {
		patch.value[DELXR_LXR_CARRIER_WAVE] = wave;
		patchAudio(tonal, &patch, waveforms[wave]);
		for (int previous = 0; previous < wave; ++previous)
			assert(memcmp(waveforms[wave], waveforms[previous], sizeof(reference)));
	}

	/* All fourteen transient selections, all eight SVF modes, all six oscillators
	 * execute production DSP, at maximum resonance/drive and both pitch extremes. */
	DelxrLxrSnare snare;
	for (int wave = 0; wave < 6; ++wave) {
		for (int filter = 0; filter < 8; ++filter) {
			for (int transient = 0; transient < 14; ++transient) {
				DelxrLxrSnareControls sweep = defaults;
				sweep.tone = (transient % 3) * 0.5f;
				delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);
				patch.value[DELXR_LXR_CARRIER_WAVE] = wave;
				patch.value[DELXR_LXR_CARRIER_TUNE] = transient & 1 ? 127 : 0;
				patch.value[DELXR_LXR_FILTER_TYPE] = filter;
				patch.value[DELXR_LXR_FILTER_RESONANCE] = 127;
				patch.value[DELXR_LXR_FILTER_DRIVE] = 127;
				patch.value[DELXR_LXR_TRANSIENT_WAVE] = transient;
				patch.value[DELXR_LXR_TRANSIENT_LEVEL] = 127;
				patch.value[DELXR_LXR_TRANSIENT_TUNE] = transient & 1 ? 0 : 127;
				patch.value[DELXR_LXR_PITCH_AMOUNT] = 127;
				patch.value[DELXR_LXR_PITCH_SLOPE] = 255;
				patch.value[DELXR_LXR_PULSE_WIDTH] = 127;
				delxr_lxr_snare_trigger_patch(&snare, &sweep, &patch);
				delxr_lxr_snare_render_patch(&snare, &sweep, transient & 1 ? 16.f : 1.f / 64.f,
				                              other, AUDIO_SIZE, &patch);
			}
		}
	}

	/* Live extras are latched to the next block, preserving already-rendered samples. */
	DelxrLxrSnare baseline, edited;
	delxr_lxr_patch_defaults(&patch, DELXR_LXR_FAMILY_SNARE);
	delxr_lxr_snare_trigger_patch(&baseline, &defaults, &patch);
	delxr_lxr_snare_trigger_patch(&edited, &defaults, &patch);
	delxr_lxr_snare_render_patch(&baseline, &defaults, 1.f, reference, 128, &patch);
	delxr_lxr_snare_render_patch(&edited, &defaults, 1.f, other, 1, &patch);
	patch.value[DELXR_LXR_CARRIER_WAVE] = 3;
	patch.value[DELXR_LXR_NOISE_TUNE] = 50;
	patch.value[DELXR_LXR_FILTER_TYPE] = 7;
	delxr_lxr_snare_render_patch(&edited, &defaults, 1.f, other + 1, 127, &patch);
	assert(!memcmp(reference, other, 64 * sizeof(int16_t)));
	assert(memcmp(reference + 64, other + 64, 64 * sizeof(int16_t)));

	/* Extreme persisted bytes are sanitized by the common schema before use. */
	memset(&patch, 255, sizeof(patch));
	patchAudio(defaults, &patch, other);
	printf("LXR snare: native patch audibility, six carriers, eight filters, fourteen transients and live edits pass.\n");
}

static int lifetime(DelxrLxrSnareControls controls, int chunk) {
	DelxrLxrSnare snare;
	int16_t out[257];
	delxr_lxr_snare_trigger(&snare, &controls);
	int samples = 0;
	while (delxr_lxr_snare_render(&snare, &controls, 1.f, out, chunk)) {
		samples += chunk;
		assert(samples < 10 * 44100);
	}
	samples += chunk;
	/* Once dead, a caller can continue rendering safely and receives silence. */
	assert(!delxr_lxr_snare_render(&snare, &controls, 1.f, out, 257));
	for (int i = 0; i < 257; ++i) assert(out[i] == 0);
	return samples;
}

int main(void) {
	nativePatchTests();
	int16_t reference[AUDIO_SIZE], split[AUDIO_SIZE], other[AUDIO_SIZE];
	audio(defaults, 1.f, reference);
	long long energy = 0;
	for (int i = 0; i < AUDIO_SIZE; ++i) energy += (long long)reference[i] * reference[i];
	assert(energy > 1000000);

	/* Each voice is independent of call order and arbitrary host render boundaries. */
	DelxrLxrSnare a, b;
	delxr_lxr_snare_trigger(&a, &defaults);
	delxr_lxr_snare_trigger(&b, &defaults);
	int offset = 0;
	while (offset < AUDIO_SIZE) {
		int count = 1 + offset % 257;
		if (count > AUDIO_SIZE - offset) count = AUDIO_SIZE - offset;
		delxr_lxr_snare_render(&a, &defaults, 1.f, split + offset, count);
		delxr_lxr_snare_render(&b, &defaults, 1.f, other + offset, count);
		offset += count;
	}
	assert(!memcmp(reference, split, sizeof(reference)));
	assert(!memcmp(reference, other, sizeof(reference)));
	delxr_lxr_snare_trigger(&a, &defaults);
	delxr_lxr_snare_render(&a, &defaults, 1.f, other, AUDIO_SIZE);
	assert(!memcmp(reference, other, sizeof(reference)));

	/* The original topology filters noise only: tone cannot alter a pure tonal hit. */
	DelxrLxrSnareControls controls = defaults;
	controls.snappy = 0.f;
	controls.tone = 0.f;
	audio(controls, 1.f, reference);
	controls.tone = 1.f;
	audio(controls, 1.f, other);
	assert(!memcmp(reference, other, sizeof(reference)));
	controls.snappy = 1.f;
	audio(controls, 1.f, reference);
	controls.tone = 0.f;
	audio(controls, 1.f, other);
	assert(memcmp(reference, other, sizeof(reference)));

	/* Tonal pitch follows Deluge pitch; noise clock transposes within its safe range. */
	audio(defaults, 1.f, reference);
	audio(defaults, 2.f, other);
	assert(memcmp(reference, other, sizeof(reference)));

	/* All four performance controls affect the sound, using native DSP parameters. */
	for (int control = 0; control < 4; ++control) {
		controls = defaults;
		switch (control) {
		case 0: controls.decay = 0.8f; break;
		case 1: controls.tone = 0.1f; break;
		case 2: controls.snappy = 0.1f; break;
		case 3: controls.clap = 1.f; break;
		}
		audio(controls, 1.f, other);
		assert(memcmp(reference, other, sizeof(reference)));
	}

	controls = defaults;
	controls.decay = 0.f;
	const int shortest = lifetime(controls, 1);
	assert(shortest == 128); /* One attack block, one silent decay block, fully consumed. */
	controls.decay = 1.f;
	const int longest = lifetime(controls, 1);
	assert(longest > 3 * 44100 && longest < 6 * 44100);
	controls.clap = 1.f;
	const int clap = lifetime(controls, 1);
	assert(clap > longest);
	assert(lifetime(controls, 257) - clap < 257);

	/* Nonfinite values cannot reach integer conversions or oscillator phase arithmetic. */
	const DelxrLxrSnareControls invalid = {NAN, INFINITY, -INFINITY, NAN};
	const DelxrLxrSnareControls zeros = {0.f, 0.f, 0.f, 0.f};
	audio(invalid, NAN, reference);
	audio(zeros, 1.f, other);
	assert(!memcmp(reference, other, sizeof(reference)));
	for (int i = 0; i < 4; ++i) {
		const float pitch[] = {INFINITY, -INFINITY, 0.f, -1.f};
		audio(zeros, pitch[i], reference);
		assert(!memcmp(reference, other, sizeof(reference)));
	}
	audio(defaults, 1.e30f, reference);
	audio(defaults, 16.f, other);
	assert(!memcmp(reference, other, sizeof(reference)));
	audio(defaults, 1.e-30f, reference);
	audio(defaults, 1.f / 64.f, other);
	assert(!memcmp(reference, other, sizeof(reference)));
	printf("LXR snare: independent voices, chunking, controls, topology, pitch and tail checks pass.\n");
	printf("Tail samples: shortest=%d longest=%d maximum clap=%d\n", shortest, longest, clap);
	return 0;
}
