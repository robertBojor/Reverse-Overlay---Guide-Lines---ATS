#!/usr/bin/env bash
# After a game update: find the engine functions in the installed executable
# and regenerate the patterns in src/plugin/engine.cpp.
#
#   tools/re/update.sh            # report only
#   tools/re/update.sh --apply    # also rewrite engine.cpp
#
# Reference build: the newest refs/amtrucks_*.exe (a copy of a build that was
# confirmed working in game). After a successful update and test drive, copy
# the new executable into refs/ as the next reference.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
refs="$root/refs"
game_exe="${GAME_EXE:-/mnt/c/Program Files (x86)/Steam/steamapps/common/American Truck Simulator/bin/win_x64/amtrucks.exe}"
reference="${REFERENCE_EXE:-$(ls -1 "$refs"/amtrucks_*.exe 2>/dev/null | sort -V | tail -1)}"

[ -f "$game_exe" ] || { echo "Game executable not found: $game_exe" >&2; exit 1; }
[ -n "$reference" ] && [ -f "$reference" ] || { echo "No reference executable in $refs" >&2; exit 1; }

# Build the two helper tools (HDE64-based decoders) with any available C compiler.
bin="$refs/bin"
mkdir -p "$bin"
cc_cmd=""
for c in cc gcc clang; do
    if command -v "$c" >/dev/null 2>&1; then cc_cmd="$c"; break; fi
done
if [ -z "$cc_cmd" ]; then
    zig="$refs/toolchain/zig-linux-x86_64-0.13.0/zig"
    if [ ! -x "$zig" ]; then
        echo "No C compiler found; downloading zig 0.13.0 into $refs/toolchain ..." >&2
        mkdir -p "$refs/toolchain"
        curl -fsSL https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz |
            tar -xJ -C "$refs/toolchain"
    fi
    cc_cmd="$zig cc"
fi
for tool in insn_mask fnscan; do
    $cc_cmd -std=c11 -O2 -I"$root/tools/re/shim" -o "$bin/$tool" "$root/tools/re/$tool.c" \
        "$root/third_party/minhook/src/hde/hde64.c" 2> >(grep -v glibc >&2)
done

# Work on a copy so the game can keep running / updating.
work="$refs/work"
mkdir -p "$work"
cp "$game_exe" "$work/amtrucks_installed.exe"
echo "reference: $(basename "$reference")"
echo "installed: $(sha256sum "$work/amtrucks_installed.exe" | cut -c1-64)"
echo

python3 -I "$root/tools/re/find_in_build.py" --old "$reference" --new "$work/amtrucks_installed.exe" \
    --insn-mask "$bin/insn_mask" --fnscan "$bin/fnscan" "$@"
