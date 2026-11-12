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
