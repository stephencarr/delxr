# Experiment 001: One sound from our own code

Date: 2026-11-11. Branch: `experiment/001-first-sound`.

## Objective

Add the smallest possible Delxr-owned sound source to the firmware, trigger it from a kit row, and hear it in the emulator. Find where a new engine type fits in the Deluge's audio engine before any LXR code comes in.

## Result

Working in the emulator. A new oscillator type, "Kick", renders a from-scratch sine kick. A one-row test kit plays it from the grid. No LXR or Elixir code was used.

## What was built

- `src/deluge/dsp/delxr/kick.h`: the engine. `delxr::Kick` is the algorithm (engine type) and holds no state; `delxr::KickVoice` is twelve bytes of per-voice state. A sine starts five times above its resting pitch, falls with a 20 ms time constant, and fades with a 90 ms time constant. The voice frees itself at -60 dB.
- A new `OscType::DELXR_KICK`, wired in at six places: the type list, the saved-file name (`delxrKick`), the oscillator type menu ("Kick"), per-voice state and trigger, the voice render loop, and the rule that marks it a one-shot (like a play-once sample, so a step needs no length).
- `sdcard/KITS/000 DELXR KICK.XML`: a one-row test kit. `./delxr run` now copies everything under `sdcard/` onto the dev SD image at each launch.

About 110 lines in total; the firmware grew by 1,056 bytes.

## Where the engine sits

The seam is the existing oscillator slot. A Deluge sound has two oscillator sources, and the DX7 engine already lives there as an oscillator type with its own per-voice state. The kick follows that pattern.

What this gives for free: each kit row is its own sound with its own voice, so engine type, voice instance and track are already separate things. The kick also passes through the row's existing volume, pan, filters, envelopes and effects, and saves and loads with kits and songs.

What it costs: the kick is rendered inside the general synth voice, so the amplitude envelope, filter and patching machinery run around it even when unused. The engine's own parameters (decay, sweep) are fixed constants; nothing in the UI edits them yet.

## Tests

Driven over the emulator's control socket, with audio recorded to a file and measured.

| Test | Outcome |
| --- | --- |
| Pressing KIT loads the test kit | Pass: OLED shows "000 DELXR KICK", one row lit. Also sorts first on the full factory card. |
| Audition pad triggers the kick | Pass |
| Four steps entered on the row, then Play | Pass: hits every 0.503 s, steady over 13 hits (four per bar at about 120 bpm) |
| Pitch sweep | Pass: about 300 Hz in the first 10 ms, 200 Hz by 30 ms, settling to 65 Hz (the kit note, middle C, two octaves down) |
| Decay | Pass: peak falls from 2,516 to 269 over 400 ms, then silence; every hit identical |
| Oscillator menu shows the new type | Pass: the row's oscillator 1 menu shows "KIC" (truncated "Kick") |

## Not verified

- **How it sounds.** Measured, not listened to. To check: see "Try it" below.
- **CPU cost per voice.** Not measured. The emulator cannot give a trustworthy figure; this needs hardware.
- Choosing "Kick" from the oscillator menu by hand. Scroll events sent to the Select encoder had no visible effect in this menu or in Experiment 000, so selection through the menu is untested. The test kit sets the type directly.
- Saving a kit or song that uses the kick, and reloading it.
- Several kick rows at once, and use in a synth track rather than a kit.

## Try it

Quit any open DelugEmu window first (it holds the SD image), then:

```sh
git checkout experiment/001-first-sound
./delxr
```

In the emulator: click **KIT** (the OLED shows "000 DELXR KICK"), click a few pads along the bottom row, click **Play**.

## Known limitations

- Files saved with the kick will not load correctly on stock firmware, which does not know `delxrKick`.
- The kick is fairly quiet next to sample kits (peak about 2,500 of 32,767 at the kit's default levels).
- `./delxr run` now fails with a clear message if the emulator is already running, because the SD image is in use.

## Recommended next experiment

**Experiment 002: four knobs on the kick.** Give the engine real parameters (decay, sweep amount, sweep time, and one more) and edit them from the hardware controls while it plays. That is the first test of "four encoders for sound editing" and decides where engine parameters live: reusing the row's existing sound parameters, or new ones owned by the engine.
