# Delxr LXR kernel preview — Mac test

Install upstream DelugEmu 0.5.7 in Applications:
https://github.com/gramster/delugemu/releases/tag/v0.5.7
Use the arm64 macOS application on Apple Silicon. Open the app once to complete
macOS approval, then quit it. No compiler or Homebrew is needed for this bundle.

Extract this ZIP into a writable folder. In Terminal, type `cd `, drag the
extracted folder into Terminal, press Return, then run:

```bash
bash Launch-Delxr.command
```

The launcher explicitly selects this bundle's firmware and its virtual SD card.
Press Tab for Session, L for Load, wait a moment, then Return to load LXR000.
Press Space to play/stop. The song contains two independent 16-step instruments:
LXR LOW (low pitch, small sweep, no FM) and LXR HIGH (higher pitch, larger sweep,
FM). Both produced sequenced PCM independently in Linux emulator tests.

The selectable oscillator is DELXR Kick. Normal Deluge pitch, envelope, filter,
effects and pad controls apply. Pulse Width is retitled LXR Pitch Sweep; oscillator
Feedback is retitled LXR FM Amount. Sweep/FM latch at note trigger. Internal
engine decay is fixed at 0.5 seconds; the Deluge amp envelope can shorten it.

This is the first tonal-drum kernel prototype, not the complete LXR instrument.
Other voice families, full parameter sets, morphing and LXR FX are not ported.
Official manual review, native Mac audio and hardware performance are unverified.
This unofficial build must not be flashed to hardware.

The image retains saved songs. Only run one emulator against it; do not mount it
in Finder while in use. Re-extract to a NEW folder for a clean card.

If audio crackles, compare with:

```bash
bash Launch-Delxr.command --audio-buffer 100
```

Report Mac/chip, macOS and emulator versions; whether both sounds are audible;
crackling/latency; control response; and whether saving, quitting and reloading
preserves the song. We have verified firmware build, boot, save/reload and PCM
capture on Linux, not native Mac or live speaker playback.

Before first launch, optional integrity check:

```bash
shasum -a 256 -c SHA256SUMS
```

The card checksum changes after the emulator writes to it.

## Source and licences

Firmware baseline c23bc2fecb38ccbdcd3c187870f527277f66e726 plus source/firmware.patch.
ELF SHA-256: c22e5308ba15c20609f3234d6f63b1883b385ef18fedbfaf53cd586b7425c884.
Complete baseline source, modified source patch, Argon dependency and build
instructions are included in source/. The patch includes copied LXR source/table
provenance, GPL-3.0-only licence and Erica Synths section 7 notices; baseline
Deluge notices and all component licences remain in the source archive.
DelugEmu itself is a separate upstream download, not redistributed here.
