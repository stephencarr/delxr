# LXR voice suite: reference and control decisions

Research date: 2026-11-11. This note records reference evidence and the agreed four-control design; it is not a hardware verification report.

## Sources actually reviewed

- **Erica Synths / Sonic Potions LXR-02 Owners Manual**, 44-page PDF, © Julian Schmidt: [dealer-hosted original manual](https://www.analoguehaven.com/erica-synths/lxr-02/manual.pdf). Read the voice architecture and controls (§5, printed pp. 9–13), synthesis modules (§8, pp. 28–34), and relevant drum-design guidance (§11, pp. 39–42). The manufacturer's indexed PDF links returned 404 through the web reader. The dealer PDF was readable there; direct download to `work/reference/` returned HTTP 403, so no local PDF was retained. This is the original manual on a mirror, not a claim that the latest September 2025 edition was retrieved.
- **Elixir** at `aadf74dbc972f2e2c896eaf7353d957c3fe8c1b4`: local `/workspace/scratch/elixir/MANUAL.md`, especially §§3, 4, 9, 17, 18, 20; and `firmware/src/voices.c` (voice state, `snare_block`, `metal_block`). [Repository](https://github.com/dmajka/elixir). Elixir is useful corroboration of a fixed-point port; the present Delxr implementation's provenance remains the upstream LXR source below.
- **Upstream LXR source** at `a43cd4fcda23ec3d577dc425261c3eb28bd5abff`: [repository](https://github.com/erica-synths/lxr-desktop-app), `Core/Src/Soundengine/DSPAudio/{DrumVoice,Snare,CymbalVoice,HiHat}.c`, corresponding headers, and the imported DSP described in [the source README](../src/deluge/dsp/lxr/README.md).
- **Deluge repository manual**: [physical controls](../website/src/content/docs/manual/device_overview/controls.mdx), [automation](../website/src/content/docs/manual/device_overview/sequencer.mdx), and [UX principles](../website/src/content/docs/development/deluge/ux_principles.md). Physical encoder positions also checked against `upstream/delugemu/src/include/hw/display/deluge_skin_controls.h`.

## Native architecture

The owner manual describes six voices on seven tracks: three drum voices, snare, cymbal and a shared closed/open hat. Drum voices support FM or mixing. Snare combines a pitched oscillator with filtered noise; the filter affects the noise branch and there is no FM. Cymbal uses a carrier and two FM modulators. Hats use that topology with separate closed/open decay settings; the closed hat chokes the open hat (§5, pp. 9–13).

Snare and cymbal amplitude envelopes offer repeats. Repeat mode replaces the attack stage; attack controls the repeat phase's duration (§8.3, pp. 30–31). Thus a clap control needs repeat timing as well as repeat count. The manual's sound-design examples distinguish hats and cymbals principally through envelope and filter settings (§11.4–11.5, p. 41).

These structures are also explicit in Elixir's `voice_t`, `snare_block` and `metal_block`: the snare noise is filtered before mixing with its tonal oscillator, while both metallic modulators are combined before feeding the carrier. Open/closed hats are a single native state with two decay values. Separate Deluge hat rows must therefore have an explicit, verified choke relationship if they are to behave like the original instrument.

## Four performance controls

These are deliberate Delxr front-panel selections from a deeper engine. Normal Deluge note/transposition, volume, pan and FX remain useful alongside them. The mappings below are the agreed wrapper interfaces; check each wrapper and the final jam kit for numerical scaling and physical assignment.

| Voice | Control 1 | Control 2 | Control 3 | Control 4 |
| --- | --- | --- | --- | --- |
| Drum | Amp decay | Pitch-envelope amount | Pitch-envelope decay | Drive |
| Snare/clap | Amp decay | Noise filter cutoff (`tone`) | Noise/tonal mix (`snappy`) | Envelope repeats + repeat timing (`clap`) |
| Cymbal | Amp decay | Both FM modulator gains | Linked modulator tuning (`ratio`) | High-pass filter cutoff (`tone`) |
| Closed hat | Short amp decay range | Both FM modulator gains | Linked modulator tuning (`ratio`) | High-pass filter cutoff (`tone`) |
| Open hat | Longer amp decay range | Both FM modulator gains | Linked modulator tuning (`ratio`) | High-pass filter cutoff (`tone`) |

Names should reveal the action. In particular, `snappy` here means noise mix, not the LXR transient generator's separate snap mode. `ratio` is the wrapper's linked FM tuning control, not a freely adjustable pair of rational frequency ratios. Descriptive popup names such as “Noise mix” and “FM tune” avoid that ambiguity.

The first four-control surface was sufficient to audition the families but insufficient for LXR-style sound design. The [native editor](lxr-sound-design.md) now exposes carrier/modulator waveforms, independent FM frequencies and gains, envelope slope, pitch envelope, transient choice/level, filter type/resonance/drive, and cymbal repeats. These additional settings are live and persistent; the four original performance controls retain automation. Full LXR FX-bus routing and kit morph remain later work.

## Physical controls and Deluge conventions

The requested left-side encoder set is precisely:

| Physical encoder | Emulator control ID | Use |
| --- | --- | --- |
| Left black horizontal | `X_ENC` | Existing horizontal navigation/zoom |
| Left black vertical | `Y_ENC` | Existing vertical navigation |
| Lower gold | `MOD_ENCODER_0` | Assigned parameter |
| Upper gold | `MOD_ENCODER_1` | Assigned parameter |

`SELECT_ENC` sits centrally and `TEMPO_ENC` on the right. Neither belongs in the engine's performance-parameter mapping. Output level is an analog level control, not a spare parameter encoder.

Use two familiar gold-knob parameter pages for the four controls. This satisfies the left-side restriction while preserving X/Y navigation. The existing experiment maps decay to the lower ATTACK/RELEASE knob, the third control to its upper knob, the second control to upper CUTOFF/RES, and the fourth control to its lower knob. Labels and the supplied kit should agree.

Preserve the documented Deluge gestures:

- An audition pad selects a kit row; Shift + audition selects it silently. Affect Entire changes the editing scope.
- Hold a note and turn an assigned gold knob to set automation from that note to the next note.
- Shift + gold-knob press deletes that parameter's automation.
- Learn + gold-knob press copies automation; Shift + Learn + gold-knob press pastes it.
- Gold-knob presses switch related modes, rather than scrolling parameter lists (UX principles).

Elixir's Shift-for-fine-adjust convention is specific to its FM-1 interface. It is not evidence for replacing Deluge modifiers; the Deluge manual even specifies Shift + Select as faster menu scrolling.

## Limits

Reading the manual establishes architecture and control semantics. It does not establish identical audio, hardware CPU capacity, perfect kit interchange, or completed emulator/hardware testing. Keep the upstream notices and commit provenance, and report measurements separately from these reference facts.
