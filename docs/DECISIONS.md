# Decisions

## 001 — First LXR tonal drum experiment (2026-11-11)

- Add an oscillator type to existing Deluge instruments. Engine state belongs
  to each voice/unison source, preserving arbitrary sequencer track architecture.
  Reuse pitch, Pulse Width and oscillator Feedback storage for tuning, sweep and
  FM; preserve existing envelope, filter and effects conventions.
- Begin with the LXR tonal-drum kernel: sine table/FM, shaped decay and snap.
  Other voice families and the complete LXR parameter system remain outside
  this slice. Parameters latch on note trigger; internal decay is fixed at 0.5 s.
- Keep the implementation as an explicit patch against the pinned firmware,
  including original LXR licence terms and provenance. The upstream checkout
  can return to its unchanged baseline without a fork or private Git commits.
- Manual-based behavior review remains outstanding because the manufacturer
  site could not be retrieved. The current mappings are source-led prototype
  decisions and do not claim manual equivalence or preset compatibility.

## 000 — Desktop foundation (2026-11-11)

- Keep the existing Delxr repository; use `experiment/000-development-foundation`.
  `main` initially points to the unmodified upstream pin commit. No push or
  hardware access is part of this experiment.
- Reference Deluge community firmware release 1.2.1 and DelugEmu 0.5.7 as pinned
  submodules. The emulator explicitly offers 1.2.1; compatibility with current
  firmware `main` is not assumed. Pin changes require another boot/input/audio check.
- Build the emulator's recorded QEMU revision using its existing integration.
  Explicitly enable GTK and PulseAudio because the Linux release bundle omits
  those backends. Limit builds to four jobs for this machine's four-CPU quota.
- Use upstream DBT toolchain 16, CMake and Ninja directly. Set `DELUGE_FW_ROOT`
  and `DBT_TOOLCHAIN_PATH` through `scripts/env.sh`. This avoids legacy DBT's
  `.git`-directory assumption for submodules and its replacement of CA trust.
- Keep the execution marker as a one-line reversible patch outside the pinned
  firmware. It changes only the unsaved song's Session OLED title. No synthesis,
  sequencing or hardware behaviour is changed.
- Use an empty 256 MiB FAT32 virtual card and the default upstream synth for
  checks. No factory content downloads are necessary. Guest writes persist in
  the image; folder-backed cards have different write-back semantics.
- Preserve upstream licences and provenance in place. LXR and Elixir are
  references only in this experiment; no substantial source from either is copied.
  Future engine type, voice instance and sequencer track must remain distinct;
  no fixed six-track architecture is adopted here.
