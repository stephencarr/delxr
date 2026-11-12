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
"${CC:-cc}" "${flags[@]}" -Isrc/deluge/dsp/lxr \
  tests/delxr/lxr_voices_test.c src/deluge/dsp/lxr/*.c -lm -o "$build_dir/lxr_voices_test"
"$build_dir/lxr_voices_test"
