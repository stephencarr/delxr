# Decisions

Decisions actually made, newest last.

## 1. Delxr is a direct fork of the Deluge community firmware

The repository's history is upstream's history; our commits sit on top. Upstream is the `upstream` git remote.

- Why: Delxr will change firmware source throughout the tree. A fork keeps upstream's history, licence and attribution intact and makes `git merge` the way to take upstream changes. A submodule would suit only if we never edited upstream files.
- Consequence: the repository is the full firmware tree. Our own files are the `delxr` script, `README.md` and `docs/experiments/`, `docs/DECISIONS.md`.

## 2. The baseline is the v1.3.0 release

Commit `156500e7c4b756840bd4b116e70a3b27c27d64bb`, tagged `v1.3.0` upstream on 2026-11-04.

- Why: it is the newest stable release, and its official binary boots in DelugEmu v0.5.7. The emulator's bundled 1.2.1 is eighteen months older; upstream `main` moves daily.
- Consequence: taking newer upstream work is a deliberate merge, retested in the emulator.

## 3. Use the installed DelugEmu as it is

`./delxr run` calls the launcher script inside the installed app, passing our binary and SD image.

- Why: the installed app already boots arbitrary `.bin` files from the command line. No rebuild of QEMU or reinstall was needed.
- Consequence: the workflow depends on DelugEmu's launcher options staying compatible. Tested with v0.5.7.

## 4. One small script, `./delxr`, wraps build and run

- Why: build and launch each need several non-obvious arguments (the emulator's internal paths, the SD image, skipping an unneeded pip step). One command keeps the loop at "edit, `./delxr`, play".
- Consequence: the script is about sixty lines of shell with no dependencies. The underlying commands are documented in the experiment notes for anyone who prefers them.

## 5. The dev SD card is a raw image in `build/`

Created once from DelugEmu's card folder and attached directly.

- Why: pointing DelugEmu at a folder makes it copy the whole card into a temporary image on every launch, which took around 20 seconds here.
- Consequence: the emulator's saved songs live in `build/delxr-sd.img`, separate from the DelugEmu app's own card.

## 6. Upstream's `.github/` directory is removed

- Why: it holds upstream's CI, website deployment, dependency-update and funding configuration. Pushed to this repository it would start running here, and we want no CI yet.
- Consequence: merges from upstream that touch `.github/` conflict and are resolved by keeping it deleted.

## 7. `main` stays a clean, buildable baseline; experiments get branches

Named `experiment/NNN-short-name`.

- Why: a known-good build is always one checkout away.
- Consequence: the Experiment 000 `DELXR` popup is on `experiment/000-dev-foundation` and is not merged.

## 8. Delxr engines plug in as oscillator types

The first engine, the kick, is a new `OscType` rendered inside the existing synth voice, the way the DX7 engine is.

- Why: it is the smallest change that makes a new engine playable (about 110 lines), and it keeps engine type, voice and track separate without new architecture. Each kit row already owns its own sound and voice.
- Consequence: an engine inherits the row's volume, pan, filters, envelopes, effects and file format, and pays for the general voice around it. If hardware measurements show that overhead matters for many drum voices, a dedicated drum type is the alternative to revisit.

## 9. Delxr DSP code lives in `src/deluge/dsp/delxr/`, in the `delxr` namespace

- Why: our own code stays easy to find and to tell apart from upstream's, which keeps merges and licence provenance clear.
- Consequence: upstream files get only the few lines needed to call into it.

## 10. Test presets live in `sdcard/` and are copied to the dev card on launch

- Why: an experiment's test kit is part of the experiment and should be versioned with it.
- Consequence: `./delxr run` writes to the SD image before each launch, so it cannot run while another emulator has the image open.

## 11. Engine controls reuse spare sound parameters

The kick's four controls are carried by per-voice parameters that a kick oscillator leaves unused (pulse width, wavetable position, two FM feedbacks), renamed in the knob popup.

- Why: every existing control surface then works with no new code: gold knobs, saving, automation, parameter locks, MIDI learn and modulation.
- Consequence: an engine gets at most four such controls per oscillator, and files and menus keep the borrowed names. An engine needing more will need parameters of its own.

## 12. LXR source is brought in as-is, in its own directory, behind one wrapper header

`src/deluge/dsp/lxr/` holds the LXR files with their copyright headers, the LXR notice, and a README recording the upstream commit and every change. The rest of Delxr includes only `delxr_lxr_drum.h`.

- Why: provenance and licence terms stay clear, later LXR updates can be diffed in, and the LXR's global names and macros cannot collide with the Deluge firmware's.
- Consequence: each LXR engine brought over needs a small wrapper. The LXR files are built as separate C files, outside the DSP library's batched build.

## 13. LXR voices are per-instance, and run at the LXR's own 64-sample block rate

The LXR's global array of three drum voices became a struct passed by pointer; each sounding Deluge voice borrows one from a pool. The wrapper renders 64 samples at a time into a per-voice buffer.

- Why: "engines are not tracks" requires any number of independent voices. Keeping the LXR's block size keeps its envelope timing without touching its code.
- Consequence: 640 bytes per sounding voice, and a little buffering in the wrapper.

## 14. Delxr is GPL-3.0-only and announces itself at startup

- Why: both follow from including LXR code, whose licence is GPL-3.0-only and requires modified versions to be marked as such at startup.
- Consequence: the firmware shows `DELXR` at startup. The project name and the "LXR drum" label still need a trademark decision.

## 15. Extend the existing oscillator seam to every native LXR family

The voice-suite branch adds independent snare/clap, cymbal, closed-hat and
open-hat oscillator types. Appended enum values preserve existing numeric values;
explicit menu ordering keeps the voices reachable on OSC1 and OSC2 whether audio
inputs are connected or not. Each sounding unison source owns a pooled state and
uses the original 64-sample LXR control clock.

- Why: this completes the synthesis families using the architecture already played in `main`, while retaining Deluge sequencing and effects.
- Consequence: the jam kit has eight independent rows rather than the LXR's fixed six instruments/seven tracks. Native Deluge choke mode gives the two hats a mutual choke relationship, with the existing held-audition exception.

## 16. Four gold-knob performance controls per voice for this slice

Keep the four spare parameter channels established in Experiment 002. The starter
kit maps them to two gold-knob pages, with action-specific OLED labels. X/Y retain
navigation; Select and Tempo receive no new engine-performance assignments.

- Why: the user's left-encoder constraint and Deluge modifier conventions make this the smallest familiar playable surface.
- Consequence: deeper waveform, transient and internal-filter controls remain future work alongside LXR effects and morphing. Parameter file/menu names stay borrowed; switching engines retains their prior values.

## 17. Noise belongs to the oscillator, and portable bounds are explicit

Every imported oscillator owns its software RNG state. Fix wavetable interpolation
look-ahead wrapping and signed FM phase conversions, and bound wrapper controls.
Disable finite-math-only and strict-aliasing assumptions for the imported C files.

- Why: noise instances must not depend on rendering order, and sanitizer/production-build behavior must agree at supported control extremes.
- Consequence: exact retriggers and arbitrary render-chunk partitioning can be tested; this is a source-based adaptation, not a claim of bit-identical LXR hardware output.

## 18. Native LXR sound design gets its own left-encoder editor

The four spare automated parameters are useful performance controls, but fixed
waveforms, linked metallic operators and hard-coded envelopes/transients cannot
deliver the LXR's sound-design range. Extend each Source with a compact native
patch and expose relevant oscillator/FM/envelope/transient/filter settings.

- Why: the user found the first voices too similar and insufficiently sculptable; the instrument needs broad transformations of an individual sound.
- Consequence: Shift + the small Volume/Pan bank button opens paired pages. X navigates, Y chooses an LXR source, gold knobs edit, and Back exits. The four primary controls retain existing automation; additional live settings save independently for OSC1/OSC2 but are not yet automation lanes. Old files opt out until edited.

## 19. Sound-design validation measures transformations as well as persistence

Exercise all oscillator waves, sampled attacks, filter modes, independent FM
operators and live edits. Restore native cutoff shaping and the snare's full
0–22 kHz noise clock; fix PWM wrap, signed PCM transient expansion, rational pitch
slope endpoints, and the bass wavetable bank selection exposed by these paths.

- Why: successful boot, nonzero PCM and saved knob values did not establish musical variety in the first prototype.
- Consequence: acoustic tests now check periodicity, upper partials, spectral change, envelope shape and independent modulation. The new DELXR SCULPT demo has distinct patches; hardware/listening assessment remains necessary and is not replaced by these measurements.
