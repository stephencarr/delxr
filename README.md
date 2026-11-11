# Delxr

An experimental drum-machine firmware project for the Synthstrom Deluge,
inspired by the LXR-02's sound design and Elixir's generative performance.
Musical enjoyment guides small, independently verifiable experiments.

Experiment 000 demonstrates an edit/build/run loop using real Deluge community
firmware and DelugEmu. A reversible OLED label patch proves our build executes.
No LXR or Elixir synthesis code has been integrated. See the
[experiment record](docs/experiments/000-development-foundation.md) for measured
results, evidence and remaining desktop audio limitations.

Upstream source is pinned as Git submodules in `upstream/firmware` and
`upstream/delugemu`. Their original licences and notices remain in those trees.
Do not flash hardware as part of this experiment.

## Linux desktop setup

Verified on Debian 13 x86_64. From this repository root:

```bash
# Host packages on a normal Debian desktop (requires administrator access):
sudo apt-get install git curl python3 python3-venv pkg-config gcc g++ \
  libglib2.0-dev libpixman-1-dev libpng-dev libgtk-3-dev libpulse-dev dosfstools mtools
# In the managed Debian cloud machine, use this rootless alternative instead:
# ./scripts/cloud-host.sh

./scripts/setup.sh
source scripts/env.sh

# DelugEmu's own QEMU integration; no replacement emulator or Delxr fork.
git -C upstream/delugemu submodule update --init --depth 1 qemu
(cd upstream/delugemu && ./scripts/integrate.sh)
(cd upstream/delugemu/qemu && ./configure --target-list=arm-softmmu \
  --disable-werror --enable-gtk --enable-pa --disable-docs)
ninja -C upstream/delugemu/qemu/build -j 4 qemu-system-arm

cmake -S upstream/firmware -B upstream/firmware/build -G 'Ninja Multi-Config' \
  -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" -DCMAKE_CROSS_CONFIGS=all \
  '-DCMAKE_DEFAULT_CONFIGS=Debug;Release' -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=TRUE
cmake --build upstream/firmware/build --config Release --target deluge -j 4

mkdir -p .cache/sdcard/{SONGS,SYNTHS,KITS,SAMPLES}
upstream/delugemu/scripts/mksd.sh .cache/sdcard .cache/deluge_sd.img
upstream/delugemu/scripts/run.sh "$PWD/upstream/firmware/build/Release/deluge.elf" \
  --sd "$PWD/.cache/deluge_sd.img" --skin-scale 50
```

The last command requires an X11/Wayland desktop and a working PulseAudio server
(including PipeWire's PulseAudio compatibility service). Click pads/buttons;
scroll over encoders. Space plays/stops, Tab selects Session, C selects Clip,
and Enter presses Select. An empty virtual card is sufficient for the default
synth; factory samples are optional. The raw card image retains guest writes.

The published DelugEmu 0.5.7 Linux bundle lacks GTK and PulseAudio backends;
build from source for the interactive desktop workflow above. Firmware output is
`upstream/firmware/build/Release/deluge.elf` (symbols) and `deluge.bin`.
Use the ELF in the emulator.

## Prove a source edit

```bash
source scripts/env.sh
git -C upstream/firmware apply ../../experiments/000-delxr-label.patch
cmake --build upstream/firmware/build --config Release --target deluge -j 4
# Launch as above, then press Tab: the unsaved song title reads DELXR.

# Automated panel and WAV capture, requiring no display or sound server:
python3 scripts/smoke.py --qemu upstream/delugemu/qemu/build/qemu-system-arm \
  --firmware upstream/firmware/build/Release/deluge.elf \
  --sd .cache/deluge_sd.img --out artifacts/smoke

# Revert the marker when finished; rebuild to restore the upstream baseline.
git -C upstream/firmware apply -R ../../experiments/000-delxr-label.patch
cmake --build upstream/firmware/build --config Release --target deluge -j 4
```

The smoke test writes screenshots, audio and `result.json`. Inspect `session.ppm`
for the title and listen to `audio.wav`. It checks rendered responses to encoder,
pad and transport inputs, and rejects silent audio; it does not test human
playability or physical hardware. Use a disposable SD image for automated tests.
Setup caches and captures are ignored by Git. No environment publication or
remote push is performed by these commands.
