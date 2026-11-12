/* Delxr LXR metal wrapper behavioral tests. SPDX-License-Identifier: GPL-3.0-only */
#include "delxr_lxr_metal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const DelxrLxrMetalControls defaults = {.45f, .6f, .5f, .6f};

static void renderChunks(DelxrLxrMetal* voice, const DelxrLxrMetalControls* controls, int16_t* out, int count) {
    const int sizes[] = {1, 7, 63, 65, 129, 33};
    for (int i = 0, s = 0; i < count; ++s) {
        int size = sizes[s % 6];
        if (size > count - i) size = count - i;
        delxr_lxr_metal_render(voice, controls, 1.f, out + i, size);
        i += size;
    }
}

static int duration(DelxrLxrMetalType type, float decay) {
    DelxrLxrMetal voice;
    DelxrLxrMetalControls controls = defaults;
    controls.decay = decay;
    int16_t samples[64];
    delxr_lxr_metal_trigger(&voice, type, &controls);
    int n = 0;
    while (delxr_lxr_metal_render(&voice, &controls, 1.f, samples, 64)) {
        assert(++n < 44100 * 30 / 64);
    }
    for (int i = 0; i < 64; ++i) assert(samples[i] == 0);
    assert(!delxr_lxr_metal_render(&voice, &controls, 1.f, samples, 64));
    for (int i = 0; i < 64; ++i) assert(samples[i] == 0);
    return n;
}

/* Compare normalized DFT power, so a volume-only difference cannot satisfy a timbre test. */
static double spectralDifference(const int16_t* a, const int16_t* b) {
    double pa[48], pb[48], totalA = 0., totalB = 0.;
    const int n = 2048;
    for (int k = 0; k < 48; ++k) {
        double ar = 0., ai = 0., br = 0., bi = 0.;
        const int bin = 3 + k * 13;
        for (int i = 0; i < n; ++i) {
            const double phase = 6.283185307179586 * bin * i / n;
            const double window = .5 - .5 * cos(6.283185307179586 * i / (n - 1));
            const double c = cos(phase) * window, s = sin(phase) * window;
            ar += a[i] * c; ai += a[i] * s;
            br += b[i] * c; bi += b[i] * s;
        }
        pa[k] = ar * ar + ai * ai; pb[k] = br * br + bi * bi;
        totalA += pa[k]; totalB += pb[k];
    }
    assert(totalA > 0. && totalB > 0.);
    double difference = 0.;
    for (int k = 0; k < 48; ++k) difference += fabs(pa[k] / totalA - pb[k] / totalB);
    return difference;
}

static void patchSound(DelxrLxrMetalType type, const DelxrLxrPatch* patch, int16_t* samples) {
    DelxrLxrMetal voice;
    delxr_lxr_metal_trigger_patch(&voice, type, &defaults, patch);
    delxr_lxr_metal_render_patch(&voice, &defaults, 1.f, samples, 4096, patch);
}

static void extendedTests(void) {
    int16_t a[4096], b[4096];
    DelxrLxrPatch cy, hat;
    delxr_lxr_patch_defaults(&cy, DELXR_LXR_FAMILY_CYMBAL);
    delxr_lxr_patch_defaults(&hat, DELXR_LXR_FAMILY_OPEN_HAT);
    patchSound(DELXR_LXR_CYMBAL, &cy, a);
    patchSound(DELXR_LXR_OPEN_HAT, &hat, b);
    const double contrast = spectralDifference(a, b);
    printf("default cymbal/hat spectral distance %.3f\n", contrast);
    assert(contrast > .25);

    const struct { int param, low, high; } cases[] = {
        {DELXR_LXR_CARRIER_WAVE, 0, 3},
        {DELXR_LXR_CARRIER_TUNE, 45, 95},
        {DELXR_LXR_CARRIER_FINE, 0, 127},
        {DELXR_LXR_PULSE_WIDTH, 15, 110},
        {DELXR_LXR_MOD1_WAVE, 0, 3},
        {DELXR_LXR_MOD2_WAVE, 0, 4},
        {DELXR_LXR_MOD1_TUNE, 48, 94},
        {DELXR_LXR_MOD2_TUNE, 54, 98},
        {DELXR_LXR_MOD1_FINE, 0, 127},
        {DELXR_LXR_MOD2_FINE, 0, 127},
        {DELXR_LXR_MOD1_GAIN, 0, 127},
        {DELXR_LXR_MOD2_GAIN, 0, 127},
        {DELXR_LXR_FILTER_TYPE, 0, 1},
        {DELXR_LXR_FILTER_RESONANCE, 0, 115},
        {DELXR_LXR_FILTER_DRIVE, 0, 127},
        {DELXR_LXR_DISTORTION, 0, 120},
        {DELXR_LXR_TRANSIENT_WAVE, 8, 11},
        {DELXR_LXR_TRANSIENT_LEVEL, 0, 127},
        {DELXR_LXR_TRANSIENT_TUNE, 0, 127}
    };
    for (unsigned j = 0; j < sizeof(cases) / sizeof(*cases); ++j) {
        DelxrLxrPatch lo = cy, hi = cy;
        const int p = cases[j].param;
        if (p == DELXR_LXR_PULSE_WIDTH) lo.value[DELXR_LXR_CARRIER_WAVE] = hi.value[DELXR_LXR_CARRIER_WAVE] = 5;
        if (p == DELXR_LXR_TRANSIENT_WAVE || p == DELXR_LXR_TRANSIENT_TUNE || p == DELXR_LXR_TRANSIENT_LEVEL) {
            lo.value[DELXR_LXR_TRANSIENT_WAVE] = hi.value[DELXR_LXR_TRANSIENT_WAVE] = 8;
            lo.value[DELXR_LXR_TRANSIENT_LEVEL] = hi.value[DELXR_LXR_TRANSIENT_LEVEL] = 127;
        }
        lo.value[p] = cases[j].low; hi.value[p] = cases[j].high;
        patchSound(DELXR_LXR_CYMBAL, &lo, a);
        patchSound(DELXR_LXR_CYMBAL, &hi, b);
        const double difference = spectralDifference(a, b);
        printf("%s spectral distance %.3f\n", delxr_lxr_patch_param_name(p), difference);
        assert(difference > .005);
        DelxrLxrMetal live, unchanged;
        delxr_lxr_metal_trigger_patch(&live, DELXR_LXR_CYMBAL, &defaults, &lo);
        delxr_lxr_metal_trigger_patch(&unchanged, DELXR_LXR_CYMBAL, &defaults, &lo);
        delxr_lxr_metal_render_patch(&live, &defaults, 1.f, a, 128, &lo);
        delxr_lxr_metal_render_patch(&unchanged, &defaults, 1.f, b, 128, &lo);
        delxr_lxr_metal_render_patch(&live, &defaults, 1.f, a, 2048, &hi);
        delxr_lxr_metal_render_patch(&unchanged, &defaults, 1.f, b, 2048, &lo);
        assert(spectralDifference(a, b) > .005);
    }

    /* Live edits take effect exactly when the cached native block is consumed. */
    for (int type = DELXR_LXR_CYMBAL; type <= DELXR_LXR_OPEN_HAT; ++type) {
        DelxrLxrMetal v1, v2;
        DelxrLxrPatch patch;
        delxr_lxr_patch_defaults(&patch, (DelxrLxrFamily)(DELXR_LXR_FAMILY_CYMBAL + type));
        delxr_lxr_metal_trigger_patch(&v1, type, &defaults, &patch);
        delxr_lxr_metal_trigger_patch(&v2, type, &defaults, &patch);
        delxr_lxr_metal_render_patch(&v1, &defaults, 1.f, a, 17, &patch);
        delxr_lxr_metal_render_patch(&v2, &defaults, 1.f, b, 17, &patch);
        DelxrLxrPatch edited = patch;
        edited.value[DELXR_LXR_MOD1_TUNE] = 41;
        edited.value[DELXR_LXR_FILTER_TYPE] = 0;
        delxr_lxr_metal_render_patch(&v1, &defaults, 1.f, a, 47, &edited);
        delxr_lxr_metal_render_patch(&v2, &defaults, 1.f, b, 47, &patch);
        assert(!memcmp(a, b, 47 * sizeof(*a)));
        delxr_lxr_metal_render_patch(&v1, &defaults, 1.f, a, 2048, &edited);
        delxr_lxr_metal_render_patch(&v2, &defaults, 1.f, b, 2048, &patch);
        assert(spectralDifference(a, b) > .05);

        /* Native patch rendering is invariant to host chunk boundaries. */
        delxr_lxr_metal_trigger_patch(&v1, type, &defaults, &patch);
        delxr_lxr_metal_trigger_patch(&v2, type, &defaults, &patch);
        delxr_lxr_metal_render_patch(&v1, &defaults, 1.f, a, 4096, &patch);
        for (int i = 0; i < 4096;) {
            int count = i % 83 + 1;
            if (count > 4096 - i) count = 4096 - i;
            delxr_lxr_metal_render_patch(&v2, &defaults, 1.f, b + i, count, &patch);
            i += count;
        }
        assert(!memcmp(a, b, sizeof(a)));
        /* Exercise all source waveform paths, live switches, filters, and PCM transients. */
        for (int wave = 0; wave < 6; ++wave) {
            patch.value[DELXR_LXR_CARRIER_WAVE] = wave;
            patch.value[DELXR_LXR_MOD1_WAVE] = (wave + 1) % 6;
            patch.value[DELXR_LXR_MOD2_WAVE] = (wave + 2) % 6;
            patch.value[DELXR_LXR_PULSE_WIDTH] = wave % 2 ? 127 : 0;
            delxr_lxr_metal_render_patch(&v2, &defaults, 16.f, b, 128, &patch);
        }
        for (int filter = 0; filter < 8; ++filter) {
            patch.value[DELXR_LXR_FILTER_TYPE] = filter;
            patch.value[DELXR_LXR_FILTER_RESONANCE] = 127;
            patch.value[DELXR_LXR_FILTER_DRIVE] = 127;
            delxr_lxr_metal_render_patch(&v2, &defaults, 1.f, b, 128, &patch);
        }
        for (int wave = 0; wave < 14; ++wave) {
            patch.value[DELXR_LXR_TRANSIENT_WAVE] = wave;
            patch.value[DELXR_LXR_TRANSIENT_LEVEL] = 127;
            patch.value[DELXR_LXR_TRANSIENT_TUNE] = wave % 2 ? 127 : 0;
            delxr_lxr_metal_trigger_patch(&v2, type, &defaults, &patch);
            delxr_lxr_metal_render_patch(&v2, &defaults, 16.f, b, 4096, &patch);
        }
        memset(&patch, 255, sizeof(patch));
        delxr_lxr_metal_trigger_patch(&v2, type, &defaults, &patch);
        delxr_lxr_metal_render_patch(&v2, &defaults, 16.f, b, 4096, &patch);
    }
    /* Low carrier tuning remains genuinely pitched, even below the source's
     * lowest wavetable bank boundary. Disabling FM/filter isolates its period. */
    DelxrLxrPatch low = cy;
    low.value[DELXR_LXR_FILTER_TYPE] = 7;
    low.value[DELXR_LXR_MOD1_GAIN] = low.value[DELXR_LXR_MOD2_GAIN] = 0;
    int lastCrossings = 0;
    for (int note = 24; note <= 60; note += 12) {
        low.value[DELXR_LXR_CARRIER_TUNE] = note;
        patchSound(DELXR_LXR_CYMBAL, &low, a);
        int crossings = 0;
        for (int i = 1; i < 4096; ++i) if ((a[i] < 0) != (a[i - 1] < 0)) ++crossings;
        printf("low carrier note %d zero crossings %d\n", note, crossings);
        assert(crossings > 0);
        if (lastCrossings) assert(crossings >= lastCrossings * 2 - 2 && crossings <= lastCrossings * 2 + 2);
        lastCrossings = crossings;
    }
    /* Attack and slope shape the time-domain envelope independently. */
    DelxrLxrPatch slow = cy;
    slow.value[DELXR_LXR_AMP_ATTACK] = 96;
    patchSound(DELXR_LXR_CYMBAL, &cy, a);
    patchSound(DELXR_LXR_CYMBAL, &slow, b);
    long long fastEnergy = 0, slowEnergy = 0;
    for (int i = 0; i < 256; ++i) {
        fastEnergy += (long long)a[i] * a[i];
        slowEnergy += (long long)b[i] * b[i];
    }
    assert(slowEnergy < fastEnergy / 10);
    slow = cy;
    slow.value[DELXR_LXR_AMP_SLOPE] = 110;
    patchSound(DELXR_LXR_CYMBAL, &slow, b);
    assert(memcmp(a, b, sizeof(a)));
    /* Repeats use the native attack-as-repeat-time stage and eventually decay. */
    DelxrLxrMetal repeating;
    cy.value[DELXR_LXR_REPEAT] = 4;
    cy.value[DELXR_LXR_AMP_ATTACK] = 40;
    delxr_lxr_metal_trigger_patch(&repeating, DELXR_LXR_CYMBAL, &defaults, &cy);
    int blocks = 0;
    while (delxr_lxr_metal_render_patch(&repeating, &defaults, 1.f, a, 64, &cy)) assert(++blocks < 20000);
    assert(blocks > duration(DELXR_LXR_CYMBAL, defaults.decay));
    puts("LXR extended metal tests passed");
}

int main(void) {
    extendedTests();
    int16_t whole[4096], chunks[4096], fresh[4096], unrelated[79];
    for (int type = DELXR_LXR_CYMBAL; type <= DELXR_LXR_OPEN_HAT; ++type) {
        DelxrLxrMetal a, b, other;
        memset(&a, 0x7e, sizeof(a)); /* initialization must not rely on pool contents */
        delxr_lxr_metal_trigger(&a, type, &defaults);
        delxr_lxr_metal_trigger(&b, type, &defaults);
        delxr_lxr_metal_trigger(&other, DELXR_LXR_OPEN_HAT, &defaults);
        delxr_lxr_metal_render(&a, &defaults, 1.f, whole, 4096);
        for (int i = 0; i < 4096; i += 64) {
            delxr_lxr_metal_render(&other, &defaults, 1.2f, unrelated, 79);
            delxr_lxr_metal_render(&b, &defaults, 1.f, chunks + i, 64);
        }
        assert(!memcmp(whole, chunks, sizeof(whole)));
        delxr_lxr_metal_trigger(&b, type, &defaults);
        renderChunks(&b, &defaults, chunks, 4096);
        assert(!memcmp(whole, chunks, sizeof(whole)));
        delxr_lxr_metal_trigger(&a, type, &defaults);
        delxr_lxr_metal_render(&a, &defaults, 1.f, fresh, 4096);
        assert(!memcmp(whole, fresh, sizeof(whole)));
        long long energy = 0;
        for (int i = 0; i < 4096; ++i) energy += (long long)whole[i] * whole[i];
        assert(energy > 1000000);
        printf("type %d energy %lld duration %.3fs\n", type, energy, duration(type, .45f) * 64. / 44100.);
        assert(duration(type, 1.f) > duration(type, 0.f));
        /* Every exposed macro must audibly affect the actual source signal path. */
        for (int p = 0; p < 4; ++p) {
            DelxrLxrMetalControls lo = defaults, hi = defaults;
            switch (p) {
            case 0: lo.decay = 0.f; hi.decay = 1.f; break;
            case 1: lo.fmAmount = 0.f; hi.fmAmount = 1.f; break;
            case 2: lo.ratio = 0.f; hi.ratio = 1.f; break;
            case 3: lo.tone = 0.f; hi.tone = 1.f; break;
            }
            delxr_lxr_metal_trigger(&a, type, &lo);
            delxr_lxr_metal_trigger(&b, type, &hi);
            delxr_lxr_metal_render(&a, &lo, 1.f, whole, 4096);
            delxr_lxr_metal_render(&b, &hi, 1.f, chunks, 4096);
            assert(memcmp(whole, chunks, sizeof(whole)));
        }
        const DelxrLxrMetalControls invalid = {NAN, INFINITY, -INFINITY, NAN};
        const float pitches[] = {NAN, INFINITY, -INFINITY, -1.f, 0.f, 1e30f, 1e-30f};
        for (unsigned p = 0; p < sizeof(pitches)/sizeof(*pitches); ++p) {
            delxr_lxr_metal_trigger(&a, type, &invalid);
            delxr_lxr_metal_render(&a, &invalid, pitches[p], whole, 4096);
        }
    }
    assert(duration(DELXR_LXR_CLOSED_HAT, .45f) < duration(DELXR_LXR_OPEN_HAT, .45f));
    puts("LXR metal tests passed");
}
