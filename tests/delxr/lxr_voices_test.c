/* SPDX-License-Identifier: GPL-3.0-only
 * Host tests of the public C voice wrappers. Compare observable PCM and lifecycle
 * behavior, without depending on private engine structs or exact sound hashes.
 */
#include "delxr_lxr_drum.h"
#include "delxr_lxr_snare.h"
#include "delxr_lxr_metal.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Family { DRUM, SNARE, CYMBAL, CLOSED_HAT, OPEN_HAT, FAMILY_COUNT };
static const char* names[] = {"drum", "snare", "cymbal", "closed hat", "open hat"};
typedef union {
    DelxrLxrDrum drum;
    DelxrLxrSnare snare;
    DelxrLxrMetal metal;
} Voice;
typedef struct { float value[4]; } Controls;
static const Controls normal = {{0.5f, 0.45f, 0.55f, 0.3f}};
static enum Family checking;
static unsigned checks;

#define CHECK(condition, message) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s: %s (line %d)\n", names[checking], message, __LINE__); \
        exit(1); \
    } \
} while (0)

static void trigger(Voice* v, enum Family family, const Controls* c) {
    if (family == DRUM) {
        DelxrLxrDrumControls controls = {c->value[0], c->value[1], c->value[2], c->value[3]};
        delxr_lxr_drum_trigger(&v->drum, &controls);
    } else if (family == SNARE) {
        DelxrLxrSnareControls controls = {c->value[0], c->value[1], c->value[2], c->value[3]};
        delxr_lxr_snare_trigger(&v->snare, &controls);
    } else {
        DelxrLxrMetalControls controls = {c->value[0], c->value[1], c->value[2], c->value[3]};
        const DelxrLxrMetalType type = family == CYMBAL ? DELXR_LXR_CYMBAL
            : family == CLOSED_HAT ? DELXR_LXR_CLOSED_HAT : DELXR_LXR_OPEN_HAT;
        delxr_lxr_metal_trigger(&v->metal, type, &controls);
    }
}

static int render(Voice* v, enum Family family, const Controls* c, float pitch, int16_t* out, int count) {
    if (family == DRUM) {
        DelxrLxrDrumControls controls = {c->value[0], c->value[1], c->value[2], c->value[3]};
        return delxr_lxr_drum_render(&v->drum, &controls, pitch, out, count);
    } else if (family == SNARE) {
        DelxrLxrSnareControls controls = {c->value[0], c->value[1], c->value[2], c->value[3]};
        return delxr_lxr_snare_render(&v->snare, &controls, pitch, out, count);
    } else {
        DelxrLxrMetalControls controls = {c->value[0], c->value[1], c->value[2], c->value[3]};
        return delxr_lxr_metal_render(&v->metal, &controls, pitch, out, count);
    }
}

static double energy(const int16_t* pcm, int count) {
    double sum = 0;
    for (int i = 0; i < count; ++i) sum += (double)pcm[i] * pcm[i];
    return sum;
}

/* Deliberately end off the internal 64-sample boundary. */
enum { SAMPLES = 32771 };
static int16_t reference[FAMILY_COUNT][SAMPLES];
static int16_t actual[SAMPLES];

static void test_reset_chunks_and_independence(enum Family family) {
    Voice v, other;
    memset(&v, 0xa5, sizeof(v));
    trigger(&v, family, &normal);
    render(&v, family, &normal, 1.f, reference[family], SAMPLES);
    CHECK(energy(reference[family], SAMPLES) > 1000000., "default patch must produce audible PCM");
    int railCount = 0;
    for (int i = 0; i < SAMPLES; ++i)
        railCount += reference[family][i] == INT16_MAX || reference[family][i] == INT16_MIN;
    CHECK(railCount < SAMPLES / 2, "default patch must not be mostly clipped");

    /* Triggering a previously used allocation must clear every audible state. */
    trigger(&v, family, &normal);
    render(&v, family, &normal, 1.f, actual, SAMPLES);
    CHECK(memcmp(actual, reference[family], sizeof(actual)) == 0, "retrigger must reset PCM exactly");

    trigger(&v, family, &normal);
    trigger(&other, family, &normal);
    const int chunks[] = {1, 63, 65, 2, 511, 17, 64, 127, 3, 1024};
    int16_t interference[257];
    unsigned chunk = 0;
    for (int offset = 0; offset < SAMPLES;) {
        int count = chunks[chunk++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (count > SAMPLES - offset) count = SAMPLES - offset;
        render(&v, family, &normal, 1.f, actual + offset, count);
        /* A second voice, restarted and transposed independently, may not consume
         * this voice's random stream or change its filter/envelope state. */
        if ((chunk % 7) == 0) trigger(&other, family, &normal);
        render(&other, family, &normal, 1.7f, interference, 257);
        offset += count;
    }
    CHECK(memcmp(actual, reference[family], sizeof(actual)) == 0,
          "arbitrary chunks and interleaved instances must preserve PCM");

    trigger(&v, family, &normal);
    Voice saved = v;
    int16_t sentinel = 1234;
    render(&v, family, &normal, 1.f, &sentinel, 0);
    render(&v, family, &normal, 1.f, &sentinel, -1);
    CHECK(sentinel == 1234, "empty/negative render must not write output");
    render(&v, family, &normal, 1.f, actual, 257);
    render(&saved, family, &normal, 1.f, interference, 257);
    CHECK(memcmp(actual, interference, sizeof(interference)) == 0, "empty render must not consume audio");
}

static void test_controls(enum Family family) {
    Voice v;
    for (int control = 0; control < 4; ++control) {
        Controls c = normal;
        c.value[control] = 0.f;
        trigger(&v, family, &c);
        render(&v, family, &c, 1.f, actual, SAMPLES);
        int16_t low[SAMPLES];
        memcpy(low, actual, sizeof(low));
        c.value[control] = 1.f;
        trigger(&v, family, &c);
        render(&v, family, &c, 1.f, actual, SAMPLES);
        if (memcmp(actual, low, sizeof(low)) == 0)
            fprintf(stderr, "Unresponsive control index: %d\n", control);
        CHECK(memcmp(actual, low, sizeof(low)) != 0, "each exposed control must change the sound");
    }
    trigger(&v, family, &normal);
    render(&v, family, &normal, 2.f, actual, SAMPLES);
    CHECK(memcmp(actual, reference[family], sizeof(actual)) != 0, "pitch ratio must transpose the voice");
}

static int completion_samples(enum Family family, Controls c) {
    Voice v;
    int16_t block[257];
    trigger(&v, family, &c);
    int count = 0, active = 1;
    double earlyEnergy = 0, lateEnergy = 0;
    while (active && count < 40 * 44100) {
        active = render(&v, family, &c, 1.f, block, 257);
        const double e = energy(block, 257);
        if (count < 4096) earlyEnergy += e;
        lateEnergy = e;
        count += 257;
    }
    CHECK(!active, "voice must finish within 40 seconds, including maximum clap repeats");
    CHECK(earlyEnergy > 0., "finite-duration patch must sound before completion");
    if (count > 4096) CHECK(lateEnergy < earlyEnergy, "tail energy must decay before completion");
    for (int i = 0; i < 4; ++i) {
        memset(block, 0x55, sizeof(block));
        CHECK(!render(&v, family, &c, 1.f, block, 257), "completed voice must stay complete");
        CHECK(energy(block, 257) == 0., "completed voice must return silence");
    }
    return count;
}

static void test_completion(enum Family family) {
    Controls c = normal;
    c.value[0] = 0.f;
    const int shortDuration = completion_samples(family, c);
    c.value[0] = 1.f;
    const int longDuration = completion_samples(family, c);
    CHECK(longDuration > shortDuration + 512, "decay control must extend audible lifetime");
    if (family == SNARE) {
        c.value[3] = 1.f;
        const int clapDuration = completion_samples(family, c);
        CHECK(clapDuration > longDuration, "maximum clap repeats must complete after the ordinary tail");
    }
    printf("  %-10s completion %.3f .. %.3f seconds\n", names[family], shortDuration / 44100., longDuration / 44100.);
}

/* float-cast-overflow sanitization detects NaN/Inf and out-of-range internal
 * sample conversions that would otherwise disappear into an int16_t output. */
static void test_extremes(enum Family family) {
    const float extremes[] = {-FLT_MAX, -1.f, 0.f, FLT_MIN, 1.f, FLT_MAX, NAN, INFINITY, -INFINITY};
    Voice v;
    int16_t pcm[1027];
    for (unsigned value = 0; value < sizeof(extremes) / sizeof(extremes[0]); ++value) {
        Controls c = {{extremes[value], extremes[value], extremes[value], extremes[value]}};
        for (unsigned pitch = 0; pitch < sizeof(extremes) / sizeof(extremes[0]); ++pitch) {
            trigger(&v, family, &c);
            render(&v, family, &c, extremes[pitch], pcm + 1, 1025);
        }
        /* Keep the other controls audible while stressing each individual input;
         * mapping all controls to zero can conceal a filter or pitch failure. */
        for (int control = 0; control < 4; ++control) {
            c = normal;
            c.value[control] = extremes[value];
            trigger(&v, family, &c);
            render(&v, family, &c, 1.f, pcm + 1, 1025);
        }
    }
    for (int combination = 0; combination < 16; ++combination) {
        Controls c;
        for (int i = 0; i < 4; ++i) c.value[i] = (combination >> i) & 1;
        trigger(&v, family, &c);
        pcm[0] = 1234;
        pcm[1026] = -2345;
        render(&v, family, &c, 16.f, pcm + 1, 1025);
        CHECK(pcm[0] == 1234 && pcm[1026] == -2345, "render must respect caller output bounds");
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("LXR public-wrapper regression tests");
    for (checking = DRUM; checking < FAMILY_COUNT; ++checking) {
        test_reset_chunks_and_independence(checking);
        test_controls(checking);
        test_completion(checking);
        test_extremes(checking);
    }
    for (checking = DRUM; checking < FAMILY_COUNT; ++checking)
        for (enum Family other = checking + 1; other < FAMILY_COUNT; ++other)
            CHECK(memcmp(reference[checking], reference[other], sizeof(actual)) != 0,
                  "different families must have different PCM");
    printf("PASS: %u checks across all five LXR families\n", checks);
    return 0;
}
