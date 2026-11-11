# LXR drum voice

Source code from the Erica Synths LXR firmware, used by Delxr's LXR drum oscillator.

- Origin: https://github.com/erica-synths/lxr-desktop-app, commit `a43cd4fcda23ec3d577dc425261c3eb28bd5abff` (2026-10-23), from `Core/Inc/Soundengine/` and `Core/Src/Soundengine/`.
- Licence: GPL-3.0-only with additional terms. `NOTICE.md` here is the LXR firmware's notice, unchanged. The names "Erica Synths", "LXR" and "LXR-02" are trademarks of Erica Synths; Delxr is not affiliated with or approved by Erica Synths.
- Brought in on 2026-11-11.

## What is unchanged

Apart from include paths: `1PoleLp`, `Decay`, `SlopeEg2`, `snapEg`, `distortion`, `transientGenerator`, `transientTables`, `wavetable`, `ResonantFilter`, `MidiNoteNumbers.h`, and `Oscillator` (one unused function disabled).

## What is changed or new

- `DrumVoice.c/.h`: every function takes a pointer to one voice instead of an index into the LXR's global array of three. LFO, pan, velocity nodes, fader and dither removed; the Deluge has its own. The signal path is the LXR's.
- `BufferTools.h`: only the three helpers the voice uses.
- `config.h`, `lxr_port.h/.c`: replace the STM32 and LXR firmware services the DSP code expects. Sample rate is 44100 Hz (the LXR runs at 44140 Hz).
- `delxr_lxr_drum.c/.h`: Delxr's wrapper. It sets up a kick patch, maps four controls using the LXR's own parameter scaling, and feeds the engine its native 64-sample blocks.
