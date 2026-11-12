# Native LXR sound-design validation

This revision addresses the narrow fixed patches in the original voice-suite
prototype. It exposes native synthesis settings through the left-encoder editor
and supplies a deliberately varied DELXR SCULPT bank. Architecture/manual
sources remain in [the reference note](lxr-voice-suite-reference.md).

## DSP checks

The host runner includes the legacy wrapper tests, focused snare and metal tests,
and the new `lxr_sculpt_test.c` acoustic/behavioral suite:

```sh
SANITIZE=1 CFLAGS='-ffast-math -fno-finite-math-only -fno-strict-aliasing' scripts/test-lxr-voices.sh
python3 -m unittest discover -s tests/delxr -p 'test_*.py'
```

The final host run passed 1,759 sculptability checks, 269 legacy checks and the
focused snare/metal suites under production-like ASan, UBSan and float-cast-overflow checks. Tests cover all
six carrier and modulator waveforms, all fourteen transient modes, all eight
filter types across cutoff/resonance/drive settings, independent metallic FM
paths, pitch/envelope changes, repeats, live changes, reset and interleaving.
The legacy entrypoints and NULL extended-patch path retain the previous wrapper
behavior under the current shared DSP. Five fixture tests cover native parameter
names/ranges, exact note records and the fresh SCULPT aliases.

Measured changes include:

- Sine periodicity near 1.0 versus pitched-noise periodicity near 0; square edge energy several times that of a sine at the same pitch.
- Drum LP-to-HP spectral centroid movement from 46 to 8,459Hz in the test patch.
- Snare noise-clock spectral centroid movement from 1,852 to 6,288Hz over selected nonzero native clock settings; zero clock is also tested.
- Independent FM oscillator waveform, tuning, amount, filter and transient changes alter the normalized spectrum both on trigger and during playback.
- Correct bass wavetable selection restores upper partials: a 55 Hz saw's centroid rises from about 135 to 176 Hz compared with the erroneous bank 4 path.

All eight authored patches produce distinct, bounded audio. Their measured
centroids are 69, 172, 333, 1,665, 3,089, 885, 7,275 and 11,214 Hz in bank order. These
measurements establish a useful range of signal behavior; they are not a claim
that the presets sound good to every listener or match particular LXR factory
kits. A 16-second host preview is retained locally at `work/lxr-sculpt-preview.wav`.

## Firmware

Full and final incremental Release builds passed with DBT toolchain 22 / Arm
GCC 14.2.1. Final text/data/BSS: 1,832,028 / 68,596 / 367,080 bytes. The existing
upstream RWX LOAD-segment linker warning remains.

Tested ELF SHA-256:
`141dc71c8e33333038732c6f31a36badfd646cff27804b5b8d7bd3ce709c0e20`.
Tested binary SHA-256:
`9366cc009dfb171f5a322a97689c0790c02f42afa9eb9146b316b84d03c3bb10`.

The candidate was built from the native-editor working tree before its commit, so its generated version contains the previous commit hash with
a dirty marker. These hashes identify the tested executable independently of
later documentation/test commits.

## Emulator

Full-kit sequencing, all 32 primary knob edits, exact 39-note persistence,
save/reload/resave and clean exit pass on DelugEmu 0.5.7 / QEMU 11.0.1. First playback
window: 464,896 stereo frames, peak 8,392/32,768, RMS 2,590.02. After primary edits and
reload: 462,848 frames, peak 8,767, RMS 2,933.14.

Both deep-editor cases pass. The harness `scripts/delxr-sculpt-smoke.py` uses
only X/Y and the left gold encoders to edit native settings while the sequencer
plays. It verifies all 28 source fields after save/reload/resave, separate silent
OSC2 edits, partial XML defaults, invalid-value clipping, unknown-field skipping,
legacy opt-out and unchanged notes/primary controls on other rows.

| Live editor case | RMS before / after | Peak before / after | Spectral-band distance |
| --- | ---: | ---: | ---: |
| FM ZAP drum | 538.29 / 476.41 | 3,381 / 3,641 | 0.614 |
| FM BELL cymbal | 308.11 / 314.62 | 1,342 / 1,524 | 1.194 |

The normalized profiles remove simple volume scaling; distance ranges 0–2.
The measurements establish more than a volume change. Captured peaks remain
below output clipping. Waveform, FM, transient, filter and separate OSC2 labels
were personally reviewed in [the OLED captures](lxr-sound-design-oled.png).
[Machine-readable results](lxr-sound-design-results.json) record actual field
changes, spectra and tested firmware hashes.

The [audio example](lxr-sound-design-demo.wav) contains emulator captures:
0–4 s drum before edits, 4.35–8.35 s after; 8.7–12.7 s cymbal before,
13.05–17.05 s after. A common gain is applied to all clips for listening, with
short edge fades and silence between them. Source captures and logs remain in
`work/sculpt-editor-drum-verified`, `work/sculpt-editor-metal-final` and
`work/sculpt-full-jam`.

Reproduce with the existing source-built DelugEmu/mtools setup:

```sh
python3 scripts/delxr-sculpt-smoke.py \
  --emulator-root /path/to/delugemu \
  --qemu /path/to/delugemu/qemu/build/qemu-system-arm \
  --firmware build/Release/deluge.elf --out work/sculpt-check --row 1
# Use --row 5 for the cymbal case.
```

The harness primes encoders before loading and reads back the OLED page/source
strip between navigation steps. This accounts for dropped simulated encoder
pulses instead of assuming a fixed click count reached the intended parameter.

## Limits

The extra native settings are live and saved with each source; the four primary
controls retain automation. The extra settings are not yet new automation or
parameter-lock lanes. Physical Deluge CPU cost and subjective listening/fidelity
have not been measured. LXR FX, kit import and morphing remain separate work.
