# LXR synthesis voices

Source code from the Erica Synths LXR firmware, used by Delxr's drum, snare/clap,
cymbal, closed-hat and open-hat oscillators.

- Origin: https://github.com/erica-synths/lxr-desktop-app, commit `a43cd4fcda23ec3d577dc425261c3eb28bd5abff` (2026-10-23), from `Core/Inc/Soundengine/` and `Core/Src/Soundengine/`.
- Licence: GPL-3.0-only with additional terms. `NOTICE.md` here is the LXR firmware's notice, unchanged. The names "Erica Synths", "LXR" and "LXR-02" are trademarks of Erica Synths; Delxr is not affiliated with or approved by Erica Synths.
- Brought in on 2026-11-11.

## Imported DSP and adaptations

Apart from include paths, the following retain the imported source: `1PoleLp`,
`SlopeEg2`, `snapEg`, `distortion`, `transientGenerator`,
`transientTables`, `wavetable`, `ResonantFilter`, and `MidiNoteNumbers.h`.

- `DrumVoice.c/.h`: every function takes a pointer to one voice instead of an index into the LXR's global array of three. LFO, pan, velocity nodes, fader and dither are supplied by Deluge. The original signal path is retained; the wrapper's fixed 64-sample block contract also allows a fixed-size scratch buffer.
- `Snare.c/.h`: the global voice becomes an explicit instance. Retains pitched sample-and-hold noise through its SVF, mixed with the tonal oscillator and transient before the amplitude envelope and distortion. Deluge supplies external modulation, velocity, pan and routing. Initialization clears the instance; negative final envelope overshoot is clamped to zero and the final mix saturates before converting to 16-bit PCM.
- `CymbalVoice.c/.h`, `HiHat.c/.h`: explicit independent instances replace global voices. Both FM modulator signals feed the carrier, followed by SVF, transient, amplitude and distortion. Original carrier gains (cymbal 1, hat 0.5) remain. Deluge supplies external modulation, velocity, pan and routing. Initialization clears all state; negative final envelope overshoot is clamped to silence. Fixed-size native-block scratch buffers replace variable-length arrays. Separate Deluge hat rows use native kit choke mode instead of sharing one global hi-hat.
- `Decay.c`: rational-shaper endpoint limits avoid zero-over-zero at maximum exponential shaping; the singular slope127 is capped126.
- `transientGenerator.c`: PCM signed8-bit samples expand by multiplication, not a negative left shift; full-buffer silence clearing handles odd sizes, and sample phase advances with an integer increment.
- `Oscillator.c/.h`: include paths changed; unused STM32-register-dependent `calcNoise()` disabled. A per-oscillator xorshift state replaces the hardware RNG without changing pitched-noise sample-and-hold. FM phase offsets convert through signed integers before unsigned phase wrap. Frequencies above the sample rate wrap explicitly; invalid frequencies produce a zero increment. Interpolation wraps its look-ahead index within each 1024-entry wavetable, including PWM, fixing an out-of-bounds read at the table edge. PWM offsets convert before unsigned phase addition. Bass and intermediate octave banks are selected from actual pitch rather than the hardware-era integer-log underflow that selected bank4 for bass notes.
- `BufferTools.h`: the three buffer helpers used by the voices. The saturating add works per sample instead of on packed pairs.
- `config.h`, `lxr_port.h/.c`: replace STM32 and LXR firmware services. Sample rate is 44100 Hz (the LXR runs at 44140 Hz). There is no shared random stream between sounding voices.

## Delxr wrappers

`delxr_lxr_drum.c/.h`, `delxr_lxr_snare.c/.h` and `delxr_lxr_metal.c/.h`
provide the only C interface included by the Deluge integration. Their opaque,
double-aligned storage is size-checked against private instance layouts. Every
voice buffers the original 64-sample blocks, accepting arbitrary Deluge render
sizes without changing the envelope clock.

The wrappers choose Delxr patches and expose four performance controls per
oscillator. `delxr_lxr_patch.c/.h` additionally defines28 source settings, family defaults, ranges and names. The native editor exposes the relevant settings as21–26 total controls per family. These are Delxr patches, not LXR factory presets.
Controls and pitch are bounded and finite-checked; maximum amplitude decay is
limited so voices finish. The drum's original mapping is retained. See
[the playing guide](../../../../docs/lxr-sound-design.md) and
[reference decisions](../../../../docs/lxr-voice-suite-reference.md).

`src/deluge/dsp/CMakeLists.txt` keeps these C files out of unity builds and disables
strict aliasing for opaque storage and finite-math-only assumptions for input
guards. Other firmware retains its existing compilation settings.

Run `scripts/test-lxr-voices.sh` from the repository root for host regressions;
`SANITIZE=1` additionally checks memory access, undefined behavior and float casts.
These tests establish implementation behavior, not sound identity to hardware or
real-time CPU capacity on a physical Deluge.


Extended `_trigger_patch` / `_render_patch` APIs read native source settings at
64-sample boundaries. NULL keeps the legacy wrapper path. Native settings include
all six carrier/modulator waveforms, independent metallic operators, drum FM/mix,
coarse/fine tuning, PWM, envelopes, all fourteen transient modes, all eight filter
modes, drive, distortion and cymbal repeats. Filter cutoff uses the original
`valueShaperF2F(value, -0.9)` formula from MidiParser/valueShaper, and the snare's
noise clock uses the original CC-to-22-kHz scaling. The Deluge supplies external
note pitch, sequencing, velocity, pan, FX and the four primary automation lanes.
