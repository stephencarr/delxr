// SPDX-License-Identifier: GPL-3.0-only
// Delxr behavioral checks for the independently instantiated LXR kernel.
#include "kick.h"
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
using deluge::dsp::lxr::KickVoice;
static std::vector<int32_t> render(KickVoice& voice, int count) {
    std::vector<int32_t> out;
    for (int i = 0; i < count; ++i) {
        int32_t sample = voice.renderSample();
        assert(std::abs(static_cast<int64_t>(sample)) <= (1LL << 28));
        out.push_back(sample);
    }
    return out;
}
int main() {
    static_assert(KickVoice::sampleRate == 44100.f);
    KickVoice a, b;
    assert(!a.active() && a.renderSample() == 0);
    a.trigger(55.f, .4f, 2.f, 0.f);
    b.trigger(55.f, .4f, 2.f, 0.f);
    auto expected = render(a, 4096);
    auto independent = render(b, 4096);
    assert(expected == independent);
    // Retrigger resets every phase/filter/control counter despite prior progress.
    a.trigger(55.f, .4f, 2.f, 0.f);
    assert(render(a, 4096) == expected);
    // Interleaving a different track must not influence another track's samples.
    KickVoice c, d, another;
    c.trigger(55.f, .4f, 2.f, 0.f); d.trigger(55.f, .4f, 2.f, 0.f);
    another.trigger(180.f, .1f, 6.f, .8f);
    for (int i = 0; i < 4096; ++i) {
        assert(c.renderSample() == d.renderSample()); another.renderSample();
    }
    b.trigger(110.f, .4f, 2.f, 0.f);
    assert(render(b, 4096) != expected);
    b.trigger(55.f, .4f, 2.f, .8f);
    assert(render(b, 4096) != expected);
    b.trigger(55.f, .4f, 0.f, 0.f);
    assert(render(b, 4096) != expected);
    // A one-shot stops and stays silent beyond its stated decay and smoothing tail.
    a.trigger(55.f, .4f, 2.f, 0.f);
    auto full = render(a, 44100);
    int peak = 0; for (auto s : full) peak = std::max(peak, std::abs(s));
    assert(peak > 1000000 && !a.active());
    for (int i = 22050; i < 44100; ++i) assert(full[i] == 0);
    // Invalid UI data is contained, not allowed into phase integer conversions.
    a.trigger(std::numeric_limits<float>::quiet_NaN(), -1.f,
              std::numeric_limits<float>::infinity(), -2.f, 1.f);
    render(a, 44100); assert(!a.active());
    a.trigger(55.f, .4f, 2.f, 0.f, 0.f);
    assert(!a.active() && a.renderSample() == 0);
    printf("LXR kernel: independence, reset, 44.1kHz, bounds, pitch/FM/sweep variation, decay tail, invalid inputs passed; state=%zu bytes\n", sizeof(KickVoice));
}
