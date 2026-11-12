#!/usr/bin/env bash
# Standalone host regression tests for the imported LXR DSP; no firmware SDK needed.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p work
build_dir=$(mktemp -d work/lxr-tests.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
flags=(-std=gnu11 -O2 -g -fno-strict-aliasing)
# Optional extra compiler flags permit checks matching the firmware toolchain.
if [[ -n "${CFLAGS:-}" ]]; then
  read -r -a extra_flags <<< "$CFLAGS"
  flags+=("${extra_flags[@]}")
fi
if [[ "${SANITIZE:-0}" == 1 ]]; then
  flags+=(-O1 -fno-omit-frame-pointer -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all)
fi
# Convert the authored JSON presets to a named-field stream for the standalone C
# tests. Python is used only for JSON parsing, not for synthesizing test audio.
python3 - "$build_dir/jam_presets.txt" <<'PY'
import json
import sys
with open("tests/delxr/jam_presets.json") as source:
    presets = json.load(source)
with open(sys.argv[1], "w") as out:
    print(len(presets), file=out)
    for preset in presets:
        print(preset["name"], file=out)
        print(preset["family"], preset["transpose"], *preset["controls"], file=out)
        print(len(preset["patch"]), file=out)
        for name, value in preset["patch"].items():
            print(name, value, file=out)
PY
for test in lxr_voices_test lxr_snare_test lxr_metal_test lxr_sculpt_test; do
  "${CC:-cc}" "${flags[@]}" -Isrc/deluge/dsp/lxr \
    "tests/delxr/$test.c" src/deluge/dsp/lxr/*.c -lm -o "$build_dir/$test"
  if [[ "$test" == lxr_sculpt_test ]]; then
    "$build_dir/$test" "$build_dir/jam_presets.txt"
  else
    "$build_dir/$test"
  fi
done
