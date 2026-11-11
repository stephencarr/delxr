# Decisions

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
