# LXR voice suite validation

The implementation is based on Delxr `main` at `91bade117`, upstream Deluge
v1.3.0. Source/manual evidence is in [the reference note](lxr-voice-suite-reference.md),
and controls and limits are in [the playing guide](lxr-voice-suite.md).

## Host tests

Executed on Linux x86_64:

```sh
scripts/test-lxr-voices.sh
SANITIZE=1 CFLAGS='-ffast-math -fno-finite-math-only -fno-strict-aliasing' scripts/test-lxr-voices.sh
python3 -m unittest discover -s tests/delxr -p 'test_*.py'
```

Both DSP runs passed 269 behavioral checks across drum, snare, cymbal, closed hat
and open hat. Another 665 extreme/corner renders exercise individual and combined
invalid control floats and pitch, plus endpoint combinations. Checks cover
nonzero bounded PCM, exact retrigger, independent interleaved noise instances,
arbitrary render sizes, control/pitch response, completion and post-completion
silence. The sanitizer run includes address, undefined-behavior and float-cast
overflow checks. All three fixture tests passed.

Measured maximum completion at the tests' other control settings: drum 24.872 s,
snare 4.528 s, cymbal 13.905 s, closed hat 0.495 s, open hat 5.379 s. Clap repeats
also finish after their repeat phase. These numbers describe selected test
settings, not universal amplitude-envelope measurements or hardware capacity.

The runner also executes the focused snare and metal tests, including the
snare's noise-only filter branch and type-specific metallic envelopes. All three
C tests passed in optimized and sanitizer builds. Coverage is of the exposed
wrapper patches; unexposed original waveform/transient combinations are not a
promise of complete upstream DSP coverage.

## Firmware build

Cross-built with DBT toolchain 22, Arm GCC 14.2.1, CMake and Ninja Multi-Config.
The repository's LXR C files retain finite guards and disable strict aliasing for
opaque state; the production-like host run checks those settings too.

```sh
DBT_TOOLCHAIN_PATH="$PWD" DELUGE_FW_ROOT="$PWD" \
  toolchain/v22/linux-x86_64/cmake/bin/cmake -S . -B build \
  -G 'Ninja Multi-Config' \
  -DCMAKE_MAKE_PROGRAM="$PWD/toolchain/v22/linux-x86_64/ninja-build/bin/ninja" \
  -DCMAKE_CROSS_CONFIGS=all '-DCMAKE_DEFAULT_CONFIGS=Debug;Release' \
  -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=TRUE
DBT_TOOLCHAIN_PATH="$PWD" DELUGE_FW_ROOT="$PWD" \
  toolchain/v22/linux-x86_64/cmake/bin/cmake --build build \
  --config Release --target deluge -j 4
```

The full and final incremental builds passed. Final text/data/BSS sizes are
1,826,748 / 68,596 / 366,760 bytes; `deluge.bin` is 1,895,436 bytes. The linker
retains the upstream RWX LOAD-segment warning. No new C/C++ warnings remain in
the final incremental build.

Tested binary SHA-256: `83f4c63f36bf8b8c477fa85751eedaa565a67b94ddfbfbe51cada8e805b5ebb7`.
Tested ELF SHA-256: `89741ba47446dd31d5f4935a2fbb367415ab3544766aaaa66778cf1b803e3191`.
These binaries were built before the implementation commit, so their generated
version reports the base hash with a dirty marker. DSP/integration source matches
implementation commit `33485948f`; subsequent fixture/test/documentation changes
do not change its executable code.

## Emulator integration

All seven final emulator cases passed: full kit, drums, snare, clap, cymbal,
closed hat and open hat. The full integration harness exercises actual firmware load, sequencing,
gold-knob editing, save, reload and a second save. It validates all eight engine
types, separate row parameters, and every note's position, duration, velocity, lift,
probability, iterance and fill. The starter pattern has row note counts
`[8, 3, 4, 4, 2, 2, 12, 4]`.

The emulator caught two fixture bugs that static XML checks alone missed:
Deluge 1.3.0 requires `noteDataWithSplitProb` for these current-version files, and
the borrowed phase-width decay parameter is unipolar. The final fixture uses
28-hex-character native note records and positive decay values; the other three
engine controls use their bipolar ranges. Both cases now have regression guards.

The full mix produced 463,872 stereo frames in each playback window. Before edits,
peak was 7,773/32,768 and RMS 2,539.21; after edits and reload, peak was 8,753 and
RMS 2,857.38. Every selected row's four controls changed, and isolated cases
verified other rows retained their parameters. The complete drum-control vectors
remain independent; native knob acceleration can legitimately make individual
values coincide after edits.

| Isolated source | First-window peak, signed 16-bit | First-window RMS |
| --- | ---: | ---: |
| Three drums | 7,187 | 1,887.46 |
| Snare | 3,432 | 379.53 |
| Clap | 1,522 | 84.02 |
| Cymbal | 1,012 | 149.13 |
| Closed hat | 766 | 75.76 |
| Open hat | 533 | 71.09 |

A separate controlled sequence triggered a sustained open hat followed by a
silent but active closed-hat row at 127 ms. The matching unchoked control used
normal mono hat polyphony. Before the closed hit, both captures measured RMS
396.1. In the 200–320 ms tail window, native choke measured RMS 0.864 versus
354.33 without choke: over 50 dB suppression to the dither floor. This verifies
actual sequenced choking, separately from merely saving the `choke` flag. Held
audition behavior is the existing Deluge exception described in the guide.

The loaded/reloaded song and representative LXR, snare/clap and metal control
labels were visually reviewed. [OLED evidence](lxr-voice-suite-oled.png) and
[compact measured results](lxr-voice-suite-results.json) are versioned here.
Raw captures and firmware-written XML remain locally in `work/voices-final-*`;
the controlled choke cases are `work/voices-choke-early` and
`work/voices-choke-early-off`. The full case is
`work/voices-final-all-verified`; its unedited groove window is frames
1,084,405–1,548,277 in `audio.wav`.

Reproduce with a source-built DelugEmu v0.5.7 / QEMU 11.0.1, `mtools` and
`dosfstools` (the harness does not require a desktop):

```sh
python3 scripts/delxr-voices-smoke.py \
  --emulator-root /path/to/delugemu \
  --qemu /path/to/delugemu/qemu/build/qemu-system-arm \
  --firmware build/Release/deluge.elf --out work/voices-check
# Add --isolate drum|snare|clap|cymbal|closed-hat|open-hat for isolated checks.
# Controlled native/off comparison uses the same harness's --choke-probe option:
# native: --isolate open-hat --choke-probe
# control: same plus --hat-choke off
python3 tests/delxr/check_choke_capture.py \
  work/voices-choke-early work/voices-choke-early-off \
  --out work/voices-choke-comparison.json
```

Encoder priming and multiple detents account for the emulator's initial encoder
state. The load discards priming edits; actual persisted changes are asserted.
The comparison checker requires an audible matching pretrigger and at least
20 dB tail suppression; the measured result exceeded that threshold.


## Limits

Emulation and host tests cannot establish physical Deluge CPU headroom, latency,
speaker playback, subjective playability, or matching an LXR-02's sound. No
physical hardware was flashed. Four performance controls per oscillator are the
implemented slice; full waveform/transient/filter editing, LXR kit import,
LXR FX and kit morph are not implemented.
