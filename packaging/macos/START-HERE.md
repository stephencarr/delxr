# Delxr — Experiment 000 Mac test

This bundle runs real Deluge firmware with one reversible change: the unsaved
song's Session OLED title reads DELXR. It is not yet a custom drum engine.
The firmware, card and automated panel/audio tests have been verified on Linux.
Native macOS behaviour and live audio are what this test is intended to verify.

## Start

1. Install DelugEmu 0.5.7 from:
   https://github.com/gramster/delugemu/releases/tag/v0.5.7
   Choose a build matching your Mac; arm64 is for Apple Silicon. The emulator
   application is a separate upstream download and is not included here.
2. Drag DelugEmu.app to Applications. Open it once to complete any macOS security
   approval. Quit it before proceeding. This test supplies its own firmware and
   virtual SD card, so factory-content downloads are not required.
3. Extract this ZIP into a writable folder, such as Downloads or Documents.
4. Open Terminal, type `cd `, drag the extracted folder into Terminal, and press
   Return. Then run:

   ```bash
   bash Launch-Delxr.command
   ```

   Double-clicking Launch-Delxr.command is also possible if Finder permits it.
   Running through Terminal avoids relying on its executable file association.

No compiler, Homebrew or physical Deluge is needed. Keep the ELF and IMG beside
Launch-Delxr.command. Do not mount the IMG in Finder while the emulator uses it.
The card is a fresh 256 MiB FAT32 image with empty SONGS, SYNTHS, KITS and SAMPLES
folders. Saved songs persist in this file. Re-extract into a NEW folder to reset
it without losing saved work. Only run one emulator against this image at a time.

To check the untouched bundle before first launch:

```bash
shasum -a 256 -c SHA256SUMS
```

The SD image checksum will naturally change after the emulator writes to it.

## Five-minute check

- Press Tab (Session): confirm the OLED says DELXR. This proves that our firmware
  is running. Press C to return to Clip view.
- Click a few pads on the bottom row to place steps. Their LEDs should change.
- Press Space: playback should start and produce sound. Press Space again to stop.
- Scroll over the TEMPO encoder: tempo should change. Try its clickable triangles
  too. Click an audition pad in the far-right column to hear the default synth.
- Press Tab, then S (Save), then Return (Select encoder push) to save a song.
  Quit and relaunch. Press Tab, L (Load), then Return to reload it. Confirm the
  song title and your steps survived.
- Play for a minute and note crackles, delay between clicking and hearing a note,
  sluggish controls, hangs or crashes. This is a light functional test, not a
  polyphony benchmark. Do not use --icount for the initial real-time audio check.

If audio crackles, repeat once with a larger buffer and report both results:

```bash
bash Launch-Delxr.command --audio-buffer 100
```

## Report back

Copy and fill this in:

- Mac model/chip and macOS version:
- DelugEmu version:
- DELXR visible: yes/no
- Pad/encoder/transport response:
- Audible sound and output device (speakers/headphones/interface):
- Crackling or noticeable delay (default buffer / 100 ms):
- Save, quit, relaunch, reload: pass/fail
- Any error text or unexpected behaviour:

## Provenance and source

Firmware baseline: SynthstromAudible/DelugeFirmware release_1_2_1,
c23bc2fecb38ccbdcd3c187870f527277f66e726. This upstream tag self-reports c1.2.0.
Marker ELF SHA-256:
7d69a5f067981c36d977bf00cf824e9164e4f205a1c8640d5c464c3755bad9ae

The firmware is GPL-3.0-or-later with separate notices for bundled components;
see FIRMWARE-LICENSE.txt and the complete baseline sources in source/firmware.tar.gz.
source/000-delxr-label.patch is the only Delxr firmware modification.
source/argon.tar.gz includes the fetched Argon v0.1.0 dependency used by the build.
source/README.md explains rebuilding; source also includes the Delxr build
instructions and setup scripts. No LXR or Elixir source is included.

This is an unofficial desktop test build. Do not flash it to hardware.
