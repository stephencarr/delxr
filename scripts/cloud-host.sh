#!/usr/bin/env bash
# Rootless Debian 13 host prerequisites for the managed cloud machine.
# A normal desktop can use apt install instead; see README.md.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source /etc/os-release
[[ "$ID-$VERSION_ID-$(uname -m)" == debian-13-x86_64 ]] || {
    echo 'This rootless host setup targets Debian 13 x86_64.' >&2; exit 1;
}
base="$root/.cache/apt"
mkdir -p "$base/lists/partial" "$base/archives/partial" "$root/.cache/sysroot"
cat > "$base/sources.list" <<'EOF'
deb https://deb.debian.org/debian trixie main
deb https://deb.debian.org/debian trixie-updates main
deb https://security.debian.org/debian-security trixie-security main
EOF
options=(-o Dir::Etc::main=/dev/null -o Dir::Etc::parts=-
    -o "Dir::Etc::sourcelist=$base/sources.list" -o Dir::Etc::sourceparts=-
    -o "Dir::State::lists=$base/lists" -o "Dir::Cache::archives=$base/archives")
/usr/bin/apt-get "${options[@]}" update
/usr/bin/apt-get "${options[@]}" -o Debug::NoLocking=1 --download-only --no-install-recommends -y install \
    libglib2.0-dev libpixman-1-dev libpng-dev libgtk-3-dev libpulse-dev ninja-build dosfstools mtools
for package in "$base"/archives/*.deb; do
    dpkg-deb -x "$package" "$root/.cache/sysroot"
done
# apt omits already-installed runtime packages. Connect their existing shared
# libraries to the locally extracted development symlinks without changing /usr.
python3 - "$root/.cache/sysroot/usr/lib/x86_64-linux-gnu" <<'PY'
from pathlib import Path
import sys
directory = Path(sys.argv[1])
for link in directory.glob('*.so'):
    if link.is_symlink() and not link.exists():
        target = link.readlink().name
        local = directory / target
        installed = Path('/usr/lib/x86_64-linux-gnu') / target
        if installed.exists() and not local.exists():
            local.symlink_to(installed)
PY
echo 'Host dependencies extracted locally; source scripts/env.sh before building.'
