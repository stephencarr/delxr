# Source this file from Bash: source scripts/env.sh
# Use the existing checkout; each cloud task is already isolated. No worktree needed.
DELXR_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export DBT_TOOLCHAIN_PATH="$DELXR_ROOT/.cache"
export DELUGE_FW_ROOT="$DELXR_ROOT/upstream/firmware"
delxr_tools="$DBT_TOOLCHAIN_PATH/toolchain/v16/linux-x86_64"
export PATH="$delxr_tools/arm-none-eabi-gcc/bin:$delxr_tools/cmake/bin:$delxr_tools/ninja-build/bin:$PATH"
# Optional rootless Debian host packages, used by the managed cloud machine.
if [[ -d "$DELXR_ROOT/.cache/sysroot/usr" ]]; then
    delxr_host="$DELXR_ROOT/.cache/sysroot"
    export PATH="$delxr_host/usr/bin:$delxr_host/usr/sbin:$PATH"
    export PKG_CONFIG_SYSROOT_DIR="$delxr_host"
    export PKG_CONFIG_LIBDIR="$delxr_host/usr/lib/x86_64-linux-gnu/pkgconfig:$delxr_host/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
    export LD_LIBRARY_PATH="$delxr_host/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
unset delxr_tools delxr_host
