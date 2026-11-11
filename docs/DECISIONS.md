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
