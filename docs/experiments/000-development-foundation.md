# Experiment 000: Development foundation

Date: 2026-11-11

## Objective

Prove that Deluge firmware can be edited, built, launched and tested entirely on the Mac, using the already-installed DelugEmu.

## Result

Achieved, with one check left for a human: hearing the audio through the speakers. Everything else below was run and observed.

## Environment

| Item | Value |
| --- | --- |
| Host | Apple Silicon (arm64), macOS 26.7.1 |
| Emulator | DelugEmu v0.5.7 (build 234a7d1) at `/Applications/DelugEmu.app`, bundling QEMU 11.0.1 |
| Firmware base | Deluge community firmware v1.3.0, commit `156500e7c4b756840bd4b116e70a3b27c27d64bb` |
| Compiler | arm-none-eabi-gcc 14.2.1 (xPack), from the firmware's own toolchain bundle v22 |
| Host tools used | git 2.39.5, curl, python3 (only to bootstrap; the build uses the bundle's Python 3.12.8) |

Nothing was installed system-wide. The toolchain bundle downloads into `toolchain/` (git-ignored) on the first build.

## How DelugEmu loads firmware

DelugEmu.app's executable is a shell script that doubles as a command-line launcher. Given a path, it boots that image; it accepts raw `.bin` and `.elf`:

```sh
/Applications/DelugEmu.app/Contents/MacOS/DelugEmu path/to/deluge.bin --sd card.img
```

Launched from Finder with no arguments, it boots the first `.bin` in `~/Library/Application Support/DelugEmu/firmware/`, which is the downloaded official 1.2.1 release. `./delxr` always passes our binary explicitly, so the two cannot be confused.

## Commands

```sh
./delxr          # build, then boot build/Release/deluge.bin in DelugEmu
./delxr build    # runs: DBT_NO_PYTHON_UPGRADE=1 ./dbt build release
./delxr run      # runs DelugEmu's run.sh with our binary and build/delxr-sd.img
```

Return to baseline: `git checkout main && ./delxr`.

Timings on this machine: clean build 1 min 55 s, rebuild after a one-line change 42 s, emulator launch a few seconds.

## Tests

The emulator was driven over QEMU's control socket (QMP): injected mouse clicks and scroll events, and framebuffer screenshots read back. Audio was checked by recording the emulator's output to a WAV file and measuring it.

| Test | Firmware | Outcome |
| --- | --- | --- |
| Emulator starts, firmware boots, OLED shows the clip view | bundled 1.2.1 | Pass |
| Pad presses light the pads; Play lights green | bundled 1.2.1 | Pass |
| Official v1.3.0 release binary boots | downloaded v1.3.0 | Pass (boot only) |
| Unmodified v1.3.0 builds from source | ours | Pass, 1,764,160-byte `deluge.bin` |
| Our baseline build boots | ours, sha256 `e66be5f2…` | Pass |
| Pads, audition pad, Play on our build | ours | Pass |
| Audio is produced when notes play | ours | Pass: silence until notes sounded, then peaks around 2,400 to 5,200 of 32,767 in the recording |
| SD card is read | ours | Pass: the synth preset "000 Rich Saw Bass" loaded from the card |
| Modified build shows `DELXR` on the OLED at startup | ours, sha256 `b4f28611…` | Pass: popup captured in screenshots for about the first second after boot |
| Gold encoder | modified | Pass: OLED showed "LPF FREQUENCY: 45" |
| Tempo encoder | modified | Pass: OLED showed "TEMPO: 121" |
| Shift plus a shortcut pad | modified | Pass: opened the LPF menu |
| Pads, Play and audio after the modification | modified | Pass |

Our binaries differ from the official release binary, as expected (build stamps), so the hash distinguishes them. The modified binary contains the string `DELXR`; the baseline does not.

The modification is one line in `src/deluge/deluge.cpp`, at the end of startup:

```cpp
display->displayPopup("DELXR"); // Experiment 000: proves this build is the one running
```

It lives on the branch `experiment/000-dev-foundation`, not on `main`.

## Not verified

- **Audible sound from the speakers.** The recording proves the firmware generates audio and the emulator outputs it; nobody listened. To check: `./delxr`, click a few pads in the grid, click Play, and listen.
- Saving to the SD card (only reading was exercised).
- The Select encoder and the two scroll encoders showed no visible change in the one view tried; they were not investigated further.
- MIDI, and the Debug build type.
- Intel Macs.

## Known limitations

- The firmware's toolchain setup also installs Python packages that the build does not use. On this machine pip points at a company package mirror that lacks one pinned version, so that step fails. `./delxr` skips it (`DBT_NO_PYTHON_UPGRADE=1`). Tasks that need those packages, such as flashing hardware over USB, will need it enabled on a machine with access to pypi.org.
- DelugEmu's audio is not a faithful performance measure: under heavy synthesis load the emulated CPU falls behind real time. CPU measurements must come from hardware.
- The 1 GB SD image sits in `build/`, so deleting `build/` removes songs saved in the emulator.
- `main` drops upstream's `.github/` directory, so merges from upstream that touch it will need those changes discarded.

## Recommended next experiment

**Experiment 001: one sound from our own code.** Add the smallest possible Delxr-owned sound source to the firmware, such as a decaying sine "kick" written from scratch, triggered from a kit row, and hear it in the emulator. This finds the right seam in the Deluge's audio engine for a new engine type before any LXR code is brought in, and measures what one voice costs.
