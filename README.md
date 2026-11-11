# Delxr

An experimental drum-machine firmware for the [Synthstrom Deluge](https://synthstrom.com/product/deluge/).

Delxr aims to make the Deluge feel like a playful, immediate drum machine: LXR-style digital drum synthesis, Elixir-style generative sequencing, and the Deluge's own grid, encoders and OLED. It is not a port of any of those instruments.

Delxr is a fork of the [Deluge community firmware](https://github.com/SynthstromAudible/DelugeFirmware). It includes source code from the Erica Synths LXR firmware, so Delxr as a whole is licensed under GPL-3.0-only, with the additional terms in [src/deluge/dsp/lxr/NOTICE.md](src/deluge/dsp/lxr/NOTICE.md) applying to that code (see [LICENSE](LICENSE)).

Delxr is an independent project. It is not affiliated with, approved by or supported by Erica Synths or Synthstrom Audible. "Erica Synths", "LXR" and "LXR-02" are trademarks of Erica Synths.

## Status

- **Experiment 000 (development foundation): done.** Firmware builds on macOS and boots in the DelugEmu emulator with one command. [Notes](docs/experiments/000-development-foundation.md)
- **Experiment 001 (one sound from our own code): done.** A from-scratch sine kick, playable from a kit row. [Notes](docs/experiments/001-first-sound.md)
- **Experiment 002 (four knobs on the kick): done.** Decay, sweep, sweep time and drive on the gold knobs. [Notes](docs/experiments/002-kick-knobs.md)
- **Experiment 003 (one LXR voice running): done in the emulator.** The LXR firmware's drum voice, playable from a kit row. CPU cost on hardware is not yet measured. [Notes](docs/experiments/003-lxr-drum-voice.md)

`main` is upstream v1.3.0 plus the dev script, docs, the kick engine and the LXR drum voice. Decisions so far are in [docs/DECISIONS.md](docs/DECISIONS.md).

## Requirements

- macOS on Apple Silicon (the only setup tested)
- git, curl, python3 (Xcode Command Line Tools is enough)
- [DelugEmu](https://github.com/gramster/delugemu/releases) installed as an app, launched once so it has downloaded its SD card contents (tested with v0.5.7)
- About 3 GB free disk: the build downloads its own ARM toolchain into `toolchain/`, and the dev SD image is 1 GB

No compiler needs installing; the first build fetches the pinned toolchain (GCC 14.2.1, CMake, Ninja).

## Build and run

```sh
git clone https://github.com/stephencarr/delxr && cd delxr
./delxr          # build, then boot the result in DelugEmu
```

That is the whole loop: edit source, run `./delxr`, play. A clean build takes about two minutes; a rebuild after a small change about 40 seconds.

```sh
./delxr build    # build only
./delxr run      # boot the last build without rebuilding
```

- The firmware binary is `build/Release/deluge.bin` (`deluge.elf` beside it has symbols).
- `./delxr run` prints the hash and build time of the binary it boots, so you can tell it is yours and not DelugEmu's bundled firmware.
- The emulator's SD card is `build/delxr-sd.img`, created on first run from DelugEmu's card folder. Songs saved in the emulator persist there; delete the file to reset it.
- Files under `sdcard/` in this repository (Delxr test kits) are copied onto that image at every launch. Quit the emulator before launching again; it holds the image while it runs.
- Extra arguments to `./delxr run` go to DelugEmu's launcher, for example `./delxr run --audio wav` to record audio to `qemu.wav`.
- The emulator window is sized to fit your main display. Set `DELXR_SKIN_SCALE` to a percentage (for example `DELXR_SKIN_SCALE=60 ./delxr run`) or `native` to override.
- To return to the stable baseline: `git checkout main && ./delxr`.

In the emulator window, click pads and buttons with the mouse, scroll over an encoder to turn it, and hold Alt while clicking to hold several controls at once (for example Shift plus a pad).

## Upstream projects

- [Deluge community firmware](https://github.com/SynthstromAudible/DelugeFirmware): the firmware Delxr is built on (GPL-3.0)
- [DelugEmu](https://github.com/gramster/delugemu): the QEMU-based Deluge emulator (GPL-2.0)
- [Erica Synths LXR](https://github.com/erica-synths/lxr-desktop-app): future drum synthesis source (GPL-3.0)
- [Elixir](https://github.com/dmajka/elixir): generative sequencing and performance reference (GPL-3.0)
