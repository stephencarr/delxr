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

Focused snare and metal tests also passed independently, including the snare's
noise-only filter branch and the type-specific metallic envelopes.

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

The full initial build passed. Final incremental build and emulator measurements
are being recorded before this branch is handed off.

## Limits

Emulation and host tests cannot establish physical Deluge CPU headroom, latency,
speaker playback, subjective playability, or matching an LXR-02's sound. No
physical hardware was flashed. Four performance controls per oscillator are the
implemented slice; full waveform/transient/filter editing, LXR kit import,
LXR FX and kit morph are not implemented.
