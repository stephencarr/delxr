# Experiment 003: One LXR voice running

Date: 2026-11-11. Branch: `experiment/003-lxr-drum-voice` (built on 002).

## Objective

Prove that the LXR firmware's own synthesis code can run inside the Deluge firmware: port the LXR drum voice (its kick and tom engine), with real per-instance state, and play it from a kit row.

## Result

Stephen played this in the emulator on 2026-11-11 and confirmed it works.

Working in the emulator. The LXR drum voice's signal path runs unmodified inside the Deluge firmware as a new oscillator type, "LXR drum", next to our from-scratch kick. The port is feasible. What it costs in CPU on real hardware is still unknown.

## What was brought in

From https://github.com/erica-synths/lxr-desktop-app at commit `a43cd4fcda23ec3d577dc425261c3eb28bd5abff`, into `src/deluge/dsp/lxr/` (see the README there):

- Unchanged apart from include paths: both oscillators and their wavetables, the pitch and amplitude envelopes, the snap envelope, the transient generator and its tables, the filter, the distortion, the one-pole smoother.
- `DrumVoice.c/.h`, changed in one structural way: every function takes a pointer to one voice. The LXR keeps three voices in a global array addressed by number; here any number of independent voices can exist. The LXR's per-voice LFO, pan, velocity routing, panel fader and dither were removed, since the Deluge has its own.

New Delxr code around it:

- `lxr_port.h/.c` and `config.h`: stand-ins for the STM32 processor intrinsics, the hardware random number generator and build settings the LXR code expects. Sample rate set to 44,100 Hz (the LXR runs at 44,140).
- `delxr_lxr_drum.c/.h`: a wrapper that sets up a kick patch, maps four controls through the LXR's own parameter scaling, and feeds the engine its native 64-sample blocks whatever block size the Deluge asks for, so envelope timing is the LXR's.
- `src/deluge/dsp/delxr/lxr_drum.*`: a pool that lends each sounding voice its 640 bytes of LXR state.
- The same seam as the kick: a new `OscType`, four controls on the four spare sound parameters, named on the OLED as "LXR decay", "LXR pitch amt", "LXR pitch dec" and "LXR drive".

The firmware grew by about 114 KB (1.76 MB to 1.88 MB), nearly all of it the LXR's wavetables and transient samples.

## Tests

| Test | Outcome |
| --- | --- |
| LXR code compiles for the Mac and renders a kick on its own | Pass: pitch falls from about 600 Hz to 55 Hz, and the voice ends itself |
| Its four controls each change the native render as expected | Pass: decay 0.25 s to 3.4 s across the range tried; pitch amount, pitch decay and drive all respond |
| Firmware builds with the LXR code | Pass, no warnings from the LXR files |
| LXR row plays in the emulator from the test kit | Pass: steady hits every 2.01 s |
| Emulator output matches the native render at the same settings | Close: same length (428 ms against 429 ms), same resting pitch (55 Hz), similar sweep (501/233/162 Hz against 601/266/138 Hz in the first 60 ms; the two measurements start at slightly different points) |
| Decay knob | Pass: longer than the 1.9 s measured window when raised; a 2 ms click at minimum |
| Pitch decay knob | Pass: OLED "LXR PITCH DEC: 50"; pitch still about 325 Hz after 200 ms |
| Pitch amount knob | Pass: OLED "LXR PITCH AMT: 50"; starting pitch about 2,000 Hz |
| Drive knob | Pass: louder, with a longer audible tail |
| Both rows together | Pass: our kick and the LXR drum on the same steps, combined peak about double |
| Startup shows `DELXR` | Pass |

## Not verified

- **CPU cost on a real Deluge.** This is the main open question. The LXR voice is floating-point, with double precision in its envelopes and smoothers, on a processor whose own firmware is fixed-point. The emulator cannot measure it.
- **Fidelity against a real LXR-02.** The code is the LXR's, but nothing here was compared with the instrument.
- Choosing "LXR drum" from the oscillator menu by hand; several LXR voices at once; fast retriggering; saving and reloading.
- The LXR's other waveforms, FM between its two oscillators, the transient samples, and the filter at anything but fully open. The code is all present; the wrapper fixes these to a plain kick patch for now.

## Known limitations and things to decide

- **Licence.** The LXR code is GPL-3.0-only with additional terms. Delxr as a whole is therefore now GPL-3.0-only rather than the Deluge firmware's "GPL-3.0 or later". The LXR notice is kept in `src/deluge/dsp/lxr/NOTICE.md`.
- **Startup marking.** The LXR terms require modified versions to show at startup that they are modified. This branch shows `DELXR` at startup for that reason.
- **Trademark.** Those terms also bar using the names "LXR" and "Erica Synths" in a way that suggests affiliation. The project name "Delxr" and the on-screen label "LXR drum" both lean on "LXR". This needs a decision from Stephen before anything is published more widely; this is not legal advice.
- The wrapper re-applies the four controls every render block and rebuilds the patch on every trigger. Simple and correct, but not the cheapest way.
- Only four controls are exposed, because that is how many spare sound parameters an oscillator has. The LXR drum voice has about twenty.

## Correction to Experiment 002

The test kit used in 002 listed two parameters twice, and the second entries reset the kick's sweep time to its minimum. So 002's "starting sound matches Experiment 001" was wrong: the starting sound had a much shorter sweep. The knobs themselves worked as reported. The kit is fixed here, and the kick row's starting sound now measures like 001's.

## Recommended next experiment

**Experiment 004: measure on hardware.** Put this build on a real Deluge and find out how many LXR drum voices play before audio breaks up, against the same number of our fixed-point kicks. Everything after this (more LXR engines, more controls, the Kick Orchestra) depends on that number. It needs Stephen's go-ahead, since it means loading firmware onto the instrument.
