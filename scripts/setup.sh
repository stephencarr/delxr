#!/usr/bin/env bash
# Fetch the pinned firmware toolchain. Run from any directory on Linux x86_64.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[[ "$(uname -s)-$(uname -m)" == Linux-x86_64 ]] || {
    echo 'This setup is verified only on Linux x86_64.' >&2; exit 1;
}
cd "$root"
git submodule update --init upstream/firmware upstream/delugemu
mkdir -p .cache/downloads .cache/toolchain/v16
archive=.cache/downloads/dbt-toolchain-16-linux-x86_64.tar.gz
if [[ ! -f "$archive" ]]; then
    curl -fL --retry 2 https://github.com/SynthstromAudible/dbt-toolchain/releases/download/v16/dbt-toolchain-16-linux-x86_64.tar.gz -o "$archive.part"
    mv "$archive.part" "$archive"
fi
echo "2316dbfc93a105512be35c985f5b2549bf1288ae1a5dc33dd495fd4fe3d6edde  $archive" | sha256sum -c -
if [[ ! -f .cache/toolchain/v16/linux-x86_64/VERSION ]]; then
    tar -xzf "$archive" -C .cache/toolchain/v16
fi
echo 'Next: source scripts/env.sh (see README.md for build commands).'
