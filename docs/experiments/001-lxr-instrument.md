# Experiment 001 — LXR tonal drum integration

## Scope

The user authorized an exploratory LXR port and asked that the LXR-02 manual
inform the design. The first implementation slice adds a selectable **DELXR
Kick** oscillator to existing Deluge synth instruments. This is a tonal-drum
subset, not a complete LXR-02 port or a verified emulation of every LXR control.

A track sequences an instrument. A voice/unison source owns its synthesis state.
Choosing the same engine type on another track does not share its envelopes,
phase or parameters. The two-track fixture demonstrates independent instances;
it does not establish a two-, six- or eight-track architectural limit.

## Manual-review status

**The official LXR-02 manual has not been retrieved or reviewed in this session.**
The attempted manufacturer product page was:

https://www.ericasynths.lv/shop/standalone-instruments-1/drum-synthesizer-lxr-02/

The ordinary HTTPS request returned HTTP 403. A request to read that page outside
the command sandbox did not complete before the agent was interrupted. The
required manufacturer domain is `www.ericasynths.lv`. No PDF URL or page-number
citations have been verified. Existing Elixir documentation is not a substitute
for the requested LXR-02 manual. Consequently, the mappings below are pragmatic
Deluge integration decisions based on inspected source, not claims about the
manual's exact parameter meanings, ranges or interaction conventions.

This remains an outstanding requirement before calling the result a
manual-guided port. No additional manual download was attempted after the abort.

## Implemented control mapping

Use an ordinary Deluge synth instrument; select **DELXR Kick** in oscillator
Type. Existing pitch, amp-envelope, filter, effect and sequencing conventions
remain the interface rather than a new LXR front-panel simulation.

| Musical control | Deluge interface / stored field | Current behavior |
| --- | --- | --- |
| Engine type | Oscillator Type; XML oscillator `type="lxr"` | Selects the experimental tonal drum source. |
| Pitch | Existing note pitch and oscillator transpose | Sets base oscillator frequency; separate tracks retain independent settings. |
| Pitch sweep | Oscillator Pulse Width page, retitled **LXR Pitch Sweep**; existing `pulseWidth` field | Trigger-sampled depth nominally 0–4; a **linear frequency-multiplier depth**, not an octave range. |
| FM depth | Existing oscillator Feedback page, retitled **LXR FM Amount**; `carrier1Feedback` / `carrier2Feedback` | Trigger-sampled amount 0–1, retaining the upstream parameter serialization. |
| Overall amplitude and tail | Existing Deluge amp envelope, velocity and level | These shape the output after the kick engine; the internal engine decay is currently fixed at 0.5 s. |
| Tone / processing | Existing Deluge filter, distortion and effects | Uses the upstream signal path; these are not ports of the LXR FX bus. |
| Sequence / transport | Existing Deluge clips, step pads, audition and Play controls | Uses the existing Deluge sequencer, not a port of the LXR pattern system. |

The kernel's sweep is applied approximately as
`frequency * (1 + pitchEnvelope * depth + snapEnvelope * 0.06 * depth)`.
The source converts the Deluge phase-width final-value range into that depth;
the fixture's serialized `0xC0000000` and `0x40000000` values produce depths 1 and
3 respectively. Carrier feedback's final-value range is normalized by 11,863,284
for the FM amount. Those internal mappings are implementation details, not a new
user-facing unit or a promise of continuous within-note FM/sweep modulation.

The fixture in `experiments/001/sdcard/SONGS/LXR000.XML` contains:

- **LXR LOW:** MIDI note 36, sweep depth 1, FM amount 0.
- **LXR HIGH:** MIDI note 43, sweep depth 3, FM amount 0.75.

Each uses one active oscillator and unison one, on a 16-step clip with four
alternating hits. Both instruments serialize their own parameters. See
`experiments/001/README.md` and the parent experiment's validation record for
commands and actual pass/fail outcomes; this document does not certify tests
that were still running when the control mapping was inspected.

## Boundaries

Other LXR engine architectures, its complete waveform/parameter set, kit
morphing, ratcheting, generative pattern behavior and its FX bus have not been
integrated by this slice. There is no physical Deluge performance measurement,
subjective sound-match result or native Mac validation here. Existing engine
provenance and licence terms are retained under `src/deluge/dsp/lxr` in the
experimental firmware patch. A displayed DELXR identifier marks the modified
firmware; it does not imply Erica Synths affiliation.

## Build and reproduce

Baseline remains Deluge `c23bc2fecb38ccbdcd3c187870f527277f66e726`;
LXR reference is `a43cd4fcda23ec3d577dc425261c3eb28bd5abff`.
DBT toolchain 16 / Arm GCC 13.2.1, CMake 3.27.9 and source-built DelugEmu 0.5.7 /
QEMU 11.0.1 are the Experiment 000 environment. No new dependency is needed.

From the Delxr repository root, after following the base README setup:

```bash
source scripts/env.sh
# Apply once to the unchanged pinned firmware checkout:
git -C upstream/firmware apply ../../experiments/001/firmware.patch
cmake --build upstream/firmware/build --config Release --target deluge -j 4
mkdir -p artifacts
cp upstream/firmware/build/Release/deluge.elf artifacts/delxr-lxr.elf
python3 scripts/lxr-smoke.py \
  --qemu upstream/delugemu/qemu/build/qemu-system-arm \
  --firmware artifacts/delxr-lxr.elf --out artifacts/lxr-smoke
```

To use the supplied test card interactively on an existing desktop/audio session:

```bash
upstream/delugemu/scripts/mksd.sh experiments/001/sdcard .cache/lxr-sd.img
upstream/delugemu/scripts/run.sh "$PWD/artifacts/delxr-lxr.elf" \
  --sd "$PWD/.cache/lxr-sd.img" --skin-scale 50
# Tab, L, Return loads LXR000; Space plays/stops.
```

To restore the baseline source before switching experiments:

```bash
git -C upstream/firmware apply -R ../../experiments/001/firmware.patch
```

The built experimental ELF is retained separately in artifacts. Rebuild after
reverting if you want a baseline ELF in the ordinary build output location.
The host kernel test command and ported-equation provenance are in
`upstream/firmware/src/deluge/dsp/lxr/PROVENANCE.md` while the patch is applied.
The patch contains all newly added source, licence notices and tables.

## Executed validation

The cross-build passed; final text/data/BSS sizes were 1,274,844 / 27,312 /
490,500 bytes. BSS grew by 55,296 bytes over the baseline. This is a linker
measurement, not a hardware performance measurement. Final experimental ELF
SHA-256: `c22e5308ba15c20609f3234d6f63b1883b385ef18fedbfaf53cd586b7425c884`.

Host behavioral tests passed instance independence and interleaving, exact
retrigger reset, pitch/FM/sweep differences, bounded output, decay to silence,
44.1 kHz configuration and invalid input handling. Upstream emulator boot tests
passed both without SD (21 IRQs) and with SD (114 IRQs), with zero data/prefetch
aborts in either case.

The emulator loaded the two-instrument fixture, played, saved to a new
`LXR000 2.XML`, and reloaded it. The firmware-written XML retained both `lxr`
engine types, independent sweep/FM values and both 16-step clips. The six-second
sequencer capture window contained 279,552 stereo frames, peak 2,064/32,768 and
513,025 nonzero channel samples. The untouched firmware failed the same test
because it saved the unrecognized engines as `triangle`; the test therefore
rejects an oscillator fallback instead of accepting mere XML file presence.

The [loaded OLED](001-evidence/loaded.png) and
[reloaded OLED](001-evidence/reloaded.png) were visually inspected, and the
[executed result](001-evidence/two-track-result.json) and
[boot log](001-evidence/boot.txt) are retained. Full logs, screenshots and WAV
captures remain under ignored `artifacts/lxr-positive/`.

Two additional emulator runs isolated the instruments by muting the opposite
clip's oscillator A (oscillator B was already muted). Low-only playback produced
282,624 frames, peak 1,875 and 490,346 nonzero samples; high-only produced 285,696
frames, peak 1,863 and 496,840 nonzero samples. The actual rewritten XML confirmed
max/min oscillator volume for the low-only case and min/max for high-only. Thus
both instances produce sequenced PCM, beyond the combined song's nonzero output.
See [low-only result](001-evidence/low-only-result.json) and
[high-only result](001-evidence/high-only-result.json). This still does not verify
subjective tone, speaker playback, latency, similarity to an LXR preset or real
Deluge CPU capacity.

The upstream source checkout was restored to the unchanged pinned baseline after
validation; the patch's clean reapplication check passed. The immutable tested
ELF remains at `artifacts/delxr-lxr.elf`. Current status is **Partial**: the first
source-led playable kernel is implemented and emulator-tested, while official
manual review, other LXR voice families and native/hardware playing tests remain
outstanding. No physical hardware was accessed, and nothing was remotely pushed
or published during this experiment.
