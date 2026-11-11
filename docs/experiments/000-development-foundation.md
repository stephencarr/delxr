# Experiment 000 — Development foundation

Date: 2026-11-11. Branch: `experiment/000-development-foundation`.

## Objective and status

**Status: Partial.** The core loop is proven; live speaker playback and human
playability are not verified in this cloud session.

Modify actual Deluge firmware, build it with a pinned toolchain, run that ELF in
DelugEmu, exercise the panel and capture audio without flashing hardware.

The core edit/build/run loop is demonstrated: the unmodified Session OLED reads
**UNSAVED**, and the rebuilt firmware reads **DELXR**. Encoder, pad and transport
responses and non-silent stereo synthesis have passed an automated black-box
check. Live speaker output and subjective playing feel still require a desktop
audio session; captured audio is not a real-time latency benchmark.

## Revisions and tools

| Component | Exact revision / version |
| --- | --- |
| Deluge firmware `release_1_2_1` | `c23bc2fecb38ccbdcd3c187870f527277f66e726` |
| DelugEmu `v0.5.7` (peeled tag) | `234a7d1499a35c16f850d9d833e89c3d38bad95e` |
| QEMU, emulator's gitlink | `6e9a825c1d4e7b62d072e99a89ecd1a74c7f0d55` (11.0.1) |
| DBT toolchain | 16, required by the pinned firmware |
| Firmware compiler | xPack Arm GCC 13.2.1, 20231009 |
| CMake | 3.27.9 (DBT bundle) |
| Ninja | 1.11.1 bundled; 1.12.1 with rootless Debian host packages |
| Host compiler | Debian GCC/G++ 14.2.0-19 |
| Python / Git | 3.12.14 / 2.52.0 |
| Host | Debian 13.6, Linux x86_64; 4 CPU quota, 32 GiB memory limit |

`release_1_2_1` internally declares CMake project version 1.2.0 and displays /
saves `c1.2.0`. That upstream version-string discrepancy does not identify a
different checkout: use the full commit SHA above.

DBT archive SHA-256 (checked by `scripts/setup.sh`):
`2316dbfc93a105512be35c985f5b2549bf1288ae1a5dc33dd495fd4fe3d6edde`.
The initially tested Linux emulator release archive had SHA-256
`e04b6b450011074936ed15c6c1090e2e12547b9e2dde32069c1e8f48312ac033`.
These are recorded download hashes, not upstream signature attestations. HTTPS,
Debian archive signatures and upstream FetchContent hashes remain enabled.

## Upstream investigation

The Delxr checkout initially had no commits or files; the origin advertised no
refs. Work stayed in that existing repository. `main` was created at the initial
pin commit; experiments and documentation are on the separate experiment branch.

- [Deluge community firmware](https://github.com/SynthstromAudible/DelugeFirmware):
  current `main` observed at `71822ff1b2bcfd330d6295c4d9689e4a0ec50413`.
  Bare-metal Cortex-A9/RZ-A1L C/C++/assembly, with instrument clips, instruments,
  sounds and voice instances already distinct (`src/deluge/model`, `processing`,
  `hid`, `gui`). Uses CMake/Ninja and the DBT cross-toolchain; produces ELF, BIN,
  HEX and map files. Core firmware notices specify GPL-3.0-or-later; bundled
  components retain their own notices. We use the older tagged release that
  DelugEmu explicitly offers, rather than assuming compatibility with `main`.
- [DelugEmu](https://github.com/gramster/delugemu): current `main` observed at
  `95e3b70ea76a8d287623fb67795c66380736a99e`. A custom QEMU machine models the
  Cortex-A9, PIC controls, GPIO encoders, OLED, pad LEDs, SSI audio and SDHI.
  It loads real firmware ELF/BIN images; it is not a native recompilation of
  the firmware. GPL-2.0-or-later, kept as a separate host program. Its default
  firmware download is release 1.2.1. The source scripts document QMP panel
  input and screenshots. Source build needs Python, Ninja, pkg-config, GLib,
  Pixman and PNG development files; GTK and PulseAudio are explicit desktop
  additions here. USB hardware integration and timing fidelity remain limited;
  TCG performance does not establish Deluge hardware capacity.
- [LXR-02 source](https://github.com/erica-synths/lxr-desktop-app): inspected at
  `a43cd4fcda23ec3d577dc425261c3eb28bd5abff`. Despite the repository name, this
  is STM32F765 Cortex-M7 firmware, not a desktop audio application. CMake >=3.21,
  Ninja, Arm GCC and Python build it. `Core/Src/Soundengine/DSPAudio` contains
  `DrumVoice`, `Snare`, `CymbalVoice`, `HiHat` and shared DSP; parameter/mixer and
  sequencer code couples these to fixed hardware voices. That layout is a
  reference, not Delxr's track architecture. GPL-3.0-only plus section 7 terms
  and ST linking permission in `NOTICE.md`; modified-version identification,
  trademark restrictions and third-party notices must be preserved later.
- [Elixir](https://github.com/dmajka/elixir): inspected at
  `aadf74dbc972f2e2c896eaf7353d957c3fe8c1b4`. M-VAVE FM-1 firmware based on
  Felucca's hardware layer; fixed-point DSP on JieLi, with a second core for
  voices. `firmware/src/seq.c` is a useful generative sequencing reference;
  engine/UI/hardware layers and seven-track product limits are not a proposed
  Deluge architecture. Its build uses the JieLi toolchain, AC79 SDK, Python and
  Pillow (Docker on non-Linux-x86_64 hosts); it also has a browser emulator.
  GPL-3.0-only; `NOTICE.md` and `LICENSES/` record LXR, Felucca and other derived
  components with their separate terms. No source or factory assets were copied.

LXR and Elixir were reviewed, not built: their target toolchains are unnecessary
for Experiment 000. No commitment to an engine or track limit was made.

## Reproduction

The [README](../../README.md) gives the tested setup, firmware build, emulator
build, launch and reversible-marker commands, all from the repository root.
`scripts/setup.sh` initializes only the two top-level submodules and downloads
the pinned toolchain with SHA-256 verification. QEMU is initialized separately
at the emulator's recorded gitlink. Do not update submodules to remote tips.

On this managed Debian machine, `scripts/cloud-host.sh` substitutes for privileged
package installation: it downloads signature-verified Debian packages, extracts
them under `.cache/sysroot`, and connects development-library links to matching
already-installed runtime libraries. Run it before sourcing `scripts/env.sh` and
configuring QEMU. System package files are not modified. Initial missing shared
library targets caused link failures; the saved helper fixes them before
configuration. Reconfiguring after that fix changed cached static-library
choices and necessarily rebuilt QEMU objects.

Build only `arm-softmmu` / `qemu-system-arm`, four jobs, GTK and PulseAudio enabled.
Use the upstream integration patches and build system. No custom emulator model,
firmware compatibility patch, CI pipeline or container is introduced.

The initial firmware build started from an empty build directory and compiled
466 steps successfully. The marker rebuild changed only `session_view.cpp` and
relinked. The source was subsequently restored with the reverse patch and rebuilt.
The restored ELF's SHA-256 was identical to the initial baseline in this same
checkout: `95e592f9f470753ecefe866bf7f8b90eb60ad2036942858bb63f514e6f5324e7`.
The marker ELF was `7d69a5f067981c36d977bf00cf824e9164e4f205a1c8640d5c464c3755bad9ae`.
This is not a claim of path-independent or cross-platform bit reproducibility.
The original baseline and modified ELF captures remain under ignored `artifacts/`.
The one-line patch is committed; there are no private upstream commits to restore.

For noninteractive validation after building:

```bash
source scripts/env.sh
python3 scripts/smoke.py --qemu upstream/delugemu/qemu/build/qemu-system-arm \
  --firmware artifacts/delxr-marker.elf --sd .cache/deluge_sd.img \
  --out artifacts/modified-source
```

`artifacts/delxr-marker.elf` is created by copying the built ELF while the marker
patch is applied. It is not distributed in Git. Fresh checkouts should apply the
patch and use the build output directly, as the README shows.

## Checks and evidence

- Baseline ELF booted, rendered the default synth Clip screen and responded to
  Tab with a Session screen reading **UNSAVED**. Compilation alone was not the check.
- Modified ELF booted and rendered **DELXR** after Tab. The title was visually
  inspected from the emulator's captured framebuffer; it is not text inserted
  into a host-side emulator overlay.
- Tempo wheel input changed the OLED BPM value; pointer input at the bottom-left
  step changed that pad's LED. Space changed the Play LED and started synthesis;
  a second Space stopped it and cleared the LED. The test checks rendered
  regions rather than merely accepting QMP commands.
- WAV capture contained non-silent 44.1 kHz, 16-bit stereo PCM. The original
  QEMU WAV output left RIFF/data sizes at zero on clean QMP quit; the test retains
  `qemu-output.wav`, validates its PCM header and rewrites only the container
  into playable `audio.wav`. It rejects silent / negligible sample output.
- The firmware mounted a blank 256 MiB FAT32 image and created `MIDIFollow.XML`.
  Session → Save → Select wrote `SONGS/SONG1.XML` (8,470 bytes), extracted and
  inspected with mtools. This verifies SD writes and the Select encoder's push
  input as well as ordinary button input. The virtual date defaults to 1980.
- The final source-built QEMU passed upstream `tests/boot.sh`: no-SD boot had
  21 IRQs, with-SD boot had 107 IRQs, and both had zero data/prefetch aborts.
  These are two executed boot cases, not skipped tests. The with-SD case used
  a separate image copy to avoid concurrent image access.
- The modified ELF passed all four panel/audio capabilities again with the
  source-built QEMU. Capture: 989,121 stereo frames; peak 4,077 / 32,768;
  1,494,353 nonzero channel samples. See [result JSON](000-evidence/modified-source-result.json).
- The documented upstream `run.sh` opened the source-built GTK application
  (1128×819 window including menubar) on Xorg's dummy display. The window
  showed **DELXR**, then Load → Select reloaded **SONG1** from the same card.
  [GTK marker window](000-evidence/desktop-marker.png),
  [reloaded song](000-evidence/desktop-loaded.png),
  [baseline OLED](000-evidence/baseline-oled.png), and
  [modified source OLED](000-evidence/source-session-oled.png) are retained.
- Setup helpers were repeated successfully; builds also support incremental
  no-change invocation. No source or lockfile changes were made to upstream
  dependencies beyond the explicitly applied and reverted marker.

Full-resolution PPM captures, logs, ELFs and audio are local outputs under
`artifacts/`; small retained screenshots and machine-readable results accompany
this record. The panel smoke check uses only Python's standard library because
upstream's pointer test hard-codes the macOS Cocoa backend. Firmware unit tests,
USB/MIDI interoperability and heavy-load audio benchmarks were not run.

## Limitations and next experiment

The source-built PulseAudio backend was tested, but this cloud session has no
running PulseAudio server. Initially it reported `XDG_RUNTIME_DIR not set`;
after supplying a private writable runtime directory, it reported
`could not stat pidfile .../pulse/pid: No such file or directory` and exited 1.
No live speaker output or remote-desktop audio stream is claimed. Do not replace
this check with a silent/null audio sink and call it speaker validation.

The GTK window was tested on Xorg's dummy display `:99`, not through a shared
desktop viewer. To repeat that cloud-only graphical check:

```bash
source scripts/env.sh
mkdir -p .cache/runtime .cache/xdg
chmod 700 .cache/runtime
Xorg :99 -config "$PWD/experiments/000-xorg.conf" \
  -logfile "$PWD/artifacts/Xorg.log" -nolisten tcp -noreset -novtswitch
# In another shell, from the same repository:
source scripts/env.sh
DISPLAY=:99 XDG_RUNTIME_DIR="$PWD/.cache/runtime" XDG_CACHE_HOME="$PWD/.cache/xdg" \
  upstream/delugemu/scripts/run.sh "$PWD/artifacts/delxr-marker.elf" \
  --sd "$PWD/.cache/deluge_sd.img" --skin-scale 50 \
  --audio "wav,path=$PWD/artifacts/desktop-output.wav"
```

The raw desktop WAV may need the same header normalization performed by the
smoke test. On a normal desktop use the README launch command with its working
audio server instead. Neither the GTK process nor Xorg is assumed to survive
a new task. No environment configuration was published or remotely pushed.

Desktop emulation is a behavioural development aid, not a cycle-accurate
performance model. No real-time latency, dense polyphony, real hardware timing,
physical controls, external MIDI or physical SD media have been validated.
No physical Deluge was accessed or flashed. The source build retains an upstream
linker warning about an RWX ELF LOAD segment; it did not prevent boot.

The smallest next musical experiment is one LXR kick engine instantiated
independently on two Deluge tracks, retaining existing sequencing and control
conventions. That would begin the Kick Orchestra without fixing its final track
count. Keep licensing provenance with any port, and seek explicit hardware
approval before measuring the resulting playable milestone on a Deluge.
