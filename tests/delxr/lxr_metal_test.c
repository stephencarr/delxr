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

int main(void) {
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
