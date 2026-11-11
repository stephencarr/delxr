#!/bin/bash
set -euo pipefail
bundle_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
app="${DELUGEMU_APP:-/Applications/DelugEmu.app}"
launcher="$app/Contents/MacOS/DelugEmu"
if [[ ! -x "$launcher" ]]; then
    echo "Install DelugEmu.app in Applications first:"
    echo "https://github.com/gramster/delugemu/releases/tag/v0.5.7"
    echo "For another location: DELUGEMU_APP=/path/to/DelugEmu.app bash Launch-Delxr.command"
    exit 1
fi
cd "$bundle_dir"
exec "$launcher" "$bundle_dir/delxr-lxr.elf" --sd "$bundle_dir/delxr-sd.img" --skin-scale 50 "$@"
