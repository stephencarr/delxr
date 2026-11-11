# LXR two-instrument fixture

`sdcard/SONGS/LXR000.XML` is based on the default song actually saved by the
pinned Deluge firmware during Experiment 000. It contains two distinct embedded
synth instruments named LXR LOW and LXR HIGH, each controlling an independently
configured oscillator of type `lxr`. Oscillator B is muted; unison is one.
Each clip is 96 internal ticks (sixteen 16th-note steps), with four alternating
hits. This is a two-track demonstration, not a fixed architecture limit.

- LXR LOW: MIDI note 36, pitch-sweep depth 1, FM amount 0.
- LXR HIGH: MIDI note 43, pitch-sweep depth 3, FM amount 0.75.

The XML uses the existing source transpose, oscillator A pulse-width and carrier
feedback serialization fields for the experimental pitch, sweep and FM controls.
The default Deluge filter/effects and amp envelope stay in the signal path.

After building the experimental firmware:

```bash
source scripts/env.sh
python3 scripts/lxr-smoke.py \
  --qemu upstream/delugemu/qemu/build/qemu-system-arm \
  --firmware artifacts/delxr-lxr.elf --out artifacts/lxr-smoke
```

This formats its own disposable FAT32 card, loads the fixture, captures playback,
saves a new `LXR000 2.XML`, and reloads it. It checks both LXR source types and
independent sweep/FM values in the actual firmware-written file, plus audio
specifically captured during a sequencer playback window. Inspect the OLED
captures and listen to audio.wav. To isolate either track while retaining both instruments and their sequences, add
`--isolate low` or `--isolate high` and choose a separate output directory. This
mutes the opposite oscillator in a private copy of the fixture, then checks that
the mute state survived firmware save. Subjective tone, live speaker audibility,
real-time latency and physical hardware performance require further testing.


## Validation on the actual experimental ELF

ELF SHA-256: `c22e5308ba15c20609f3234d6f63b1883b385ef18fedbfaf53cd586b7425c884`.
All cases saved and reloaded the two instruments, independent sweep/FM values,
and two sixteen-step clips. The OLED was visually inspected as LXR000 after
loading and LXR000 2 after reloading the firmware-written song.

| Case | Playback frames | Playback PCM peak (s16) | Nonzero channel samples |
| --- | ---: | ---: | ---: |
| Both instruments | 279,552 | 2,064 | 513,025 |
| Low instrument only | 282,624 | 1,875 | 490,346 |
| High instrument only | 285,696 | 1,863 | 496,840 |

The isolated runs each produced meaningful PCM during the sequencer window.
Their saved source-volume states were independently inspected as `[max, mute]`
and `[mute, max]`. This checks separate instrument signal production; it does
not measure human-perceived live latency or establish hardware capacity.
Dither can make nominally silent samples nonzero, so the test also requires a
peak above 100; the nonzero counts alone are not treated as proof of sound.

A negative control with the original Experiment 000 ELF loaded this same fixture
and saved both oscillator types as `triangle`, causing the engine-persistence
assertion to fail. Thus an old firmware fallback cannot pass this test merely
because the input XML contains the word lxr. Early driver attempts caught UI
settling and Save-as naming assumptions and were corrected before positive runs.

Local evidence: `artifacts/lxr-positive`, `artifacts/lxr-isolate-low`, and
`artifacts/lxr-isolate-high`; each contains screenshots, generated audio and
result.json. The source patch must be applied before rebuilding the experimental
firmware; see the main Experiment 001 record.
