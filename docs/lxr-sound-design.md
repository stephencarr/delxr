# LXR sound design on Delxr

The first voice-suite prototype exposed four performance controls around mostly
fixed patches. This revision opens the native oscillators, FM, transients,
envelopes and internal filters: 21–26 controls per voice family. It keeps the
Deluge sequencer and uses the left-side encoders for sound editing.

## Start with the new sounds

Build and launch this branch with `./delxr`, then load **DELXR SCULPT**. Its
standalone kit is **002 DELXR SCULPT**. This is a new demo name so it is easy to
distinguish from saved copies of the earlier fixed-patch jam.

| Row, bottom to top | Starting character |
| --- | --- |
| 1 | LOW THUMP: bass drum and sampled attack |
| 2 | FM ZAP: pitched FM percussion |
| 3 | ROUGH TOM: harmonic drum body |
| 4 | BODY SNARE: oscillator plus filtered noise |
| 5 | FILTER CLAP: repeating noise envelope |
| 6 | FM BELL: independently tuned FM operators |
| 7 | NOISE TICK: short, bright hat |
| 8 | SIZZLE: longer, noisy metallic hat |

The updated **DELXR JAM** uses the same patches and two-bar, 118 BPM pattern.
Old saved sounds without native settings retain their previous patch until a
native setting is edited. Their LIVE pages show the new native starting settings;
the first native edit initializes that configuration, so other formerly fixed
settings can also change at that point. Loading the new demo gives a fully
specified patch from the start.

## Edit with the left encoders

1. In Clip view with Affect Entire off, tap an audition pad to select the sound,
   then release it.
2. Hold **Shift** and press the small **Volume/Pan page button**, the leftmost
   round parameter-bank button between the gold knobs.
3. Turn the **left X encoder** to move between paired parameter pages.
4. The **upper and lower gold knobs** edit the upper and lower displayed values.
5. The **left Y encoder** selects LXR oscillator 1 or 2 when both are available.
6. Use audition pads to hear/select rows, or leave the sequencer playing. Press
   **Back** to exit; save the kit or song through the normal Deluge workflow.

The first two PLAY pages use the four existing automated controls. They retain
Deluge note-region editing, automation recording, Shift + gold press to delete
automation, Learn + gold press to copy, and Shift + Learn + gold press to paste.
The additional LIVE pages edit settings shared by that sound's active voices and
save them with its kit/song. These additional settings are not yet automation or
parameter-lock lanes. Opening the editor alone does not change an old sound.

The ordinary gold-bank assignments are restored after editing the PLAY pages.
Select and Tempo are not sound-editing encoders in this editor. The small bank
buttons jump to its first eight pages; X reaches every page.

## Sound-design pages

Only pages relevant to the selected family appear.

| Pages | What changes |
| --- | --- |
| PLAY 1 / PLAY 2 | Existing decay, pitch/FM/noise/filter performance controls, according to family |
| OSC SHAPE / OSC TUNE | Carrier sine, triangle, saw, square, noise or PWM; pulse width; coarse and fine tune |
| FM1 SHAPE / FM1 TUNE | First modulator waveform, amount, coarse and fine tune |
| FM ROUTING | Drum voice: frequency modulation or oscillator mixing |
| FM2 SHAPE / FM2 TUNE | Metallic voices: second independent modulator waveform, amount and tuning |
| AMPLITUDE | Native attack/repeat time and amplitude-envelope slope |
| PITCH SHAPE / PITCH MOD | Native pitch-envelope slope; snare pitch amount and decay |
| TRANSIENT / TRANSIENT TUNE | Snap, phase offset and twelve sampled attacks; level and frequency |
| NOISE | Snare's pitched sample-and-hold noise clock, from stopped to 22 kHz |
| FILTER / FILTER SHAPE | Internal LP, HP, BP, unity BP, notch, peak, LP2 or bypass; cutoff, resonance and drive |
| DIST / REPEAT | Snare/metal distortion and cymbal envelope repeats, where relevant |

Pulse width affects PWM; it has no effect on a sine wave. Sampled transients
need a nonzero level. For the snare, the internal filter affects only the noise
branch, preserving the tonal body. The drum's drive remains on PLAY 2.

On metallic voices, PLAY's FM master scales the two independent modulator gains;
FM tune shifts both independently configured tunings together around its middle
position. To remove one operator, set its amount to zero. The pitch played by
Deluge and the ordinary oscillator transpose still transpose the sound.

For snare/clap, zero native attack keeps the earlier automatic short clap timing;
a nonzero attack explicitly sets repeat timing. Large attack/repeat values can
produce long rattles. Native slope endpoints that divide by zero are capped at
126; amplitude decay remains finite.

## Try mangling one sound

- **FM ZAP:** find FM1 SHAPE, raise the amount and change the modulator waveform.
  On FM1 TUNE, sweep its coarse tune independently of the carrier. Toggle FM
  ROUTING between FM and Mix, then change the pitch envelope on PLAY.
- **ROUGH TOM:** select PWM on OSC SHAPE and sweep the lower pulse-width knob.
  Add a sampled transient and level, then try LP2 with resonance and drive.
- **FM BELL:** detune FM1 and FM2 independently. Turn one amount down, then bring
  it back with a different waveform; this changes the spectrum rather than
  simply transposing a fixed patch. Try distortion after shaping the filter.
- **BODY SNARE:** bring up noise mix, lower NOISE's clock for coarse digital
  noise, then move the internal filter between HP, BP and notch. Change pitch
  amount/decay to turn the tonal body into a zap.

The two hat rows use native Deluge mutual choke. A held audition pad is exempt
from choking in Clip view, so release it or sequence the hits to hear the normal
relationship.

Hear [captured before/after examples](lxr-sound-design-demo.wav): drum before at
0–4 seconds, after at 4.35–8.35; cymbal before at 8.7–12.7, after at 13.05–17.05.
One common listening gain is used throughout.

See [validation](lxr-sound-design-validation.md) for executed checks and remaining
limits. Native LXR effects, kit import and kit morphing are separate future work.
