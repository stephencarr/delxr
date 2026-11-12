# LXR voices

Source code from the Erica Synths LXR firmware, used by Delxr's LXR oscillator type.

- Origin: https://github.com/erica-synths/lxr-desktop-app, commit `a43cd4fcda23ec3d577dc425261c3eb28bd5abff` (2026-10-23), from `Core/Inc/` and `Core/Src/`.
- Licence: GPL-3.0-only with additional terms. `NOTICE.md` here is the LXR firmware's notice, unchanged. The names "Erica Synths", "LXR" and "LXR-02" are trademarks of Erica Synths; Delxr is not affiliated with or approved by Erica Synths.

## Unchanged apart from include paths

`1PoleLp`, `Decay`, `SlopeEg2`, `snapEg`, `distortion`, `transientGenerator`, `transientTables`, `wavetable`, `ResonantFilter`, `MidiNoteNumbers.h`, `valueShaper.h`, `Parameters.h` (the LXR's parameter list), `InitKit.h` and `initKit.c` (the LXR's built-in init kit), and `Oscillator` (one unused function disabled).

## Changed

- `DrumVoice`, `Snare`, `CymbalVoice`, `HiHat`: every function takes a pointer to one voice instead of using the LXR's global voices, so any number of independent voices can exist. The LFO member, panning, velocity modulation nodes, panel fader and dither are removed. The signal path is the LXR's.
- `BufferTools.h`: only the helpers the voices use; the saturating add works per sample instead of on packed pairs.
- `config.h`, `lxr_port.h/.c`: stand-ins for the STM32 and LXR firmware services the code expects. Sample rate is 44100 Hz (the LXR runs at 44140 Hz).

## Delxr's wrapper

`delxr_lxr_voice.c/.h` plays one voice from an LXR kit's parameter bytes. It sets each parameter the way the LXR's `MidiParser.c` does when a kit loads, and applies the LXR mixer's per-voice sample-rate reduction. It renders the LXR's native 64-sample blocks whatever block size the Deluge asks for, so envelope timing is the LXR's.

Each voice runs the LXR's LFO (`lfo.c`'s waveforms, rates and tempo sync), moving its target parameter around the kit's value once per block, as `modNode_updateValue()` does. Differences from the LXR: an LFO can only target a parameter of its own voice, and it starts from its phase offset at each hit (on the LXR a free-running LFO keeps going between hits). Not yet applied: velocity modulation.

## Factory kits from CLATTER

`sdcard/KITS/002`–`009` are the eight factory kits of [CLATTER](https://github.com/clickysteve/clatter) (GPL-3.0-only, commit `f9ddb05b`), another port of the same LXR firmware. CLATTER does not store them as values; it generates each from a fixed seed with its kit generator, starting from this init kit. The values here were produced by running CLATTER's own `buildPreset()` once and copying the resulting LXR parameter bytes into Deluge kit files, one row per LXR voice. Rendered through CLATTER's compiled engine and through this port, 42 of the 56 voices match within 15% on loudness over time, and the rest within 35% apart from two whose LFO is free-running on the LXR. Noise-based voices differ in detail because the two ports use different random streams.
