# Playing the LXR voice suite

This branch extends the LXR drum voice already in `main` with the native LXR
snare/clap, cymbal and hi-hat signal paths. It uses Deluge kit rows, sequencing,
automation, modulation and effects. All engine performance controls in the
supplied kit are on the two left gold knobs. The left black X/Y encoders keep
Deluge navigation and zoom behavior.

## Start a jam

Build and launch with the existing Mac workflow:

```sh
./delxr
```

Load the song **DELXR JAM**. It contains an eight-row kit and a two-bar groove at
118 BPM. Press Play, select a row with its audition pad, then use the gold-knob
pages below. The standalone kit is **001 DELXR JAM**. Both are copied from
`sdcard/` onto the development card by `./delxr run`; quit the emulator before
launching again.

| Row, from bottom to top | Sound |
| --- | --- |
| 1 | LXR kick |
| 2 | LXR low tom, one octave above the kick |
| 3 | LXR high tom, a fifth above the low tom |
| 4 | Snare |
| 5 | Clap, using the snare engine's repeat envelope |
| 6 | Cymbal |
| 7 | Closed hat |
| 8 | Open hat |

These are independent instruments, not fixed engine slots. The three tonal drums
use the existing LXR drum engine at different pitches and settings. The original
**000 DELXR** experiment kit remains available.

## Shape the sound

Select a small round parameter-page button between the gold knobs. These engine
assignments replace that page's printed functions for this kit; OLED popups show
the actual parameter name.

| Voice | ATTACK / RELEASE lower | ATTACK / RELEASE upper | CUTOFF / RES upper | CUTOFF / RES lower |
| --- | --- | --- | --- | --- |
| Drum | Decay | Pitch envelope decay | Pitch envelope amount | Drive |
| Snare/clap | Decay | Noise/tonal mix | Noise bandpass cutoff | Clap repeats and timing |
| Cymbal | Decay | FM modulator tuning | FM amount | High-pass cutoff |
| Closed/open hat | Decay | FM modulator tuning | FM amount | High-pass cutoff |

The other supplied pages provide native volume/pan, filter, delay, reverb and
pitch controls. Count the small page buttons from the left: button 4 gives LPF
cutoff (upper) and resonance (lower), button 6 gives delay rate (upper) and amount
(lower), and button 7 gives pitch on the lower knob. The OLED labels identify the
assigned functions, which can differ from the printed panel captions. Select and
Tempo are not assigned new engine-performance parameters. There is no new
modifier system: hold a note and turn a gold knob for Deluge note automation;
Shift + gold press deletes automation; Learn + gold press copies it;
Shift + Learn + gold press pastes it. Shift + audition selects a row silently.

Decay stays finite throughout the range. Closed-hat decay covers a shorter range
than open-hat or cymbal decay. The clap knob is off at zero; above zero it selects
one through four short envelope repeats with coupled repeat timing. Noise mix
means the balance between filtered noise and the tonal snare body; it is not the
LXR transient generator's separate snap envelope. FM tuning moves the two
modulators together, preserving their interval.

The two hat rows use Deluge's native `choke` polyphony mode. Either hat chokes the
other, whereas the LXR specifically shares one hat voice and documents the closed
hat cutting off the open one. Deluge intentionally exempts a row whose audition
pad is still held in clip/automation views: release the pad or sequence the hits
to hear the normal choke behavior.

## Scope and next work

This is a playable selection of every native LXR synthesis family. Four primary
controls are exposed per oscillator; the existing drum remains its kick-oriented
patch. Carrier/modulator waveforms, separate FM gains/tunings, transient selection,
internal envelope slope, cymbal repeats and the full internal filter controls are
not yet exposed. Native Deluge effects are available; the LXR FX bus, LXR kit
import and kit morphing are future work. No factory kits or sample dependencies
are required by the jam fixture.

The four controls use the same spare Deluge sound parameters as `main`, retaining
Deluge's save, automation and modulation paths. Their file/menu names remain
`oscAPulseWidth`, `oscAWavetablePosition`, `carrier1Feedback` and
`modulator1Feedback` (gold assignments use `oscAPhaseWidth` for the first of these).
Switching oscillator types reuses those stored values, so loading the supplied
kit is the easiest way to get suitable starting sounds.

See [source/manual evidence](lxr-voice-suite-reference.md) for architecture and
provenance, and [validation](lxr-voice-suite-validation.md) for actual test results.
Physical Deluge CPU capacity, human playability and similarity to an LXR-02 still
need hardware/listening assessment.
