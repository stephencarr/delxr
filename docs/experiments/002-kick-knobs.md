# Experiment 002: Four knobs on the kick

Date: 2026-11-11. Branch: `experiment/002-kick-knobs`.

## Objective

Give the kick engine real parameters and edit them from the Deluge's gold knobs while it plays. Decide where engine parameters live.

## Result

Working in the emulator. The kick has four controls, each on a gold knob in the test kit, each named on the OLED as it is turned, and each audibly (measurably) changing the sound while the pattern plays.

## The controls

| Control | Range | Gold knob in the test kit |
| --- | --- | --- |
| Kick decay | 20 ms to 1.2 s | ATTACK / RELEASE page, lower knob |
| Sweep time | 5 ms to 200 ms | ATTACK / RELEASE page, upper knob |
| Kick sweep | none to 16 times the note's pitch | CUTOFF / RES page, upper knob |
| Kick drive | clean to 8 times gain, soft-clipped | CUTOFF / RES page, lower knob |

Select a page with the small round button under its label, then turn the two gold knobs. The kit's starting values reproduce the Experiment 001 sound.

## Where the parameters live

The four controls ride on existing per-voice sound parameters that a kick oscillator otherwise leaves unused: pulse width, wavetable position, and the two FM feedback amounts. No new parameter was added to the firmware.

What this gives for free: gold-knob assignment, the value popup, saving and loading in kits and songs, per-step parameter locks and automation, MIDI learn, and modulation by LFOs, envelopes and velocity through the existing patching.

What it costs: only four such spare parameters exist per oscillator, the stored names in files are the borrowed ones (`oscAPulseWidth` and so on), and the sound-editor menus still show the borrowed names. Only the knob popup is renamed.

Changes: `src/deluge/dsp/delxr/kick.h` (controls and drive), about 15 lines in `voice.cpp` to read the parameters, about 30 lines in `param.cpp` for the names, and the test kit.

## Tests

Driven over the emulator's control socket with one kick looping every two seconds, audio recorded and measured. Each knob was turned and the following hits compared.

| Test | Outcome |
| --- | --- |
| Starting sound matches Experiment 001 | Pass: same pitch curve, rings about 200 ms |
| Kick decay up to maximum | Pass: OLED "KICK DECAY: 50"; rings past 1.9 s (was 0.2 s) |
| Kick decay down to minimum | Pass: rings 42 ms |
| Sweep time up to maximum | Pass: OLED "SWEEP TIME: 50"; pitch still at about 325 Hz after 60 ms (was 75 Hz) |
| Kick sweep up to maximum | Pass: OLED "KICK SWEEP: 50"; starting pitch about 1,000 Hz (was 300 Hz) |
| Kick drive up to maximum | Pass: OLED "KICK DRIVE: 50"; added harmonics and a longer audible tail |
| Changes apply while playing | Pass: the next hit after each turn already differed |

## Not verified

- **How it feels and sounds to play.** Measured, not listened to.
- CPU cost. The render now uses a little floating-point setup per block; hardware will tell.
- Saving and reloading a kit after turning the knobs, parameter locks per step, and modulating a control from an LFO or velocity. These should work through the existing machinery but were not tried.
- The sound-editor menus for these parameters.

## Known limitations

- Each scroll click moves a knob a long way in the emulator: 40 clicks ran a control from its starting value to maximum. Fine control may be easier on hardware.
- The first build of this experiment had the decay knob mapped under the wrong name in the kit, so it did nothing. Knob lists use `oscAPhaseWidth`; the value itself is stored as `oscAPulseWidth`.
- Wavetable position and pulse width are also used by other oscillator types, so switching a row's oscillator away from Kick leaves those values behind.

## Recommended next experiment

**Experiment 003: several kicks at once.** Eight kit rows, each a differently tuned and shaped kick, with a pattern on each: the Kick Orchestra in its simplest form. It tests independent voice instances side by side, shows whether the test-kit approach scales to a playable instrument, and is the first build worth measuring on hardware.
