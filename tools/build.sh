#!/usr/bin/env bash
# Build from WSL with the Windows MSVC toolchain.
# MSVC cannot build from a \\wsl$ path reliably, so the tree is mirrored to a
# Windows-side directory first.
#   tools/build.sh [Release|Debug] [targets...]
set -euo pipefail

config="${1:-Release}"
shift || true
targets=("$@")

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
win_user="$(cmd.exe /c 'echo %USERNAME%' 2>/dev/null | tr -d '\r')"
mirror="/mnt/c/Users/${win_user}/ats-reverse-overlay-build"
mirror_win="C:\\Users\\${win_user}\\ats-reverse-overlay-build"
vs="C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools"

# Create the mirror from Windows: WSL-created directories are case-sensitive
# and cannot be a Windows working directory. (Avoid AppData: packaged hosts
# virtualise it, so WSL and cmd.exe would see different folders.)
[ -d "$mirror" ] || (cd /mnt/c && cmd.exe /c "mkdir ${mirror_win}")
# rsync's rename-into-place is unreliable on drvfs; a fresh copy is fast enough.
rm -rf "$mirror/src"
mkdir -p "$mirror/src"
(cd "$project_dir" && tar --exclude=./out --exclude=./.git -cf - .) | tar -C "$mirror/src" -xf -

target_args=""
if [ ${#targets[@]} -gt 0 ]; then
    target_args="--target ${targets[*]}"
fi

cat > "$mirror/build.bat" <<EOF
@echo off
call "${vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul || exit /b 1
cmake -S "${mirror_win}\\src" -B "${mirror_win}\\build-${config}" -G Ninja -DCMAKE_BUILD_TYPE=${config} ${RTO_CMAKE_ARGS:--DRTO_BUILD_PLUGIN=ON} >nul || exit /b 1
cmake --build "${mirror_win}\\build-${config}" ${target_args} || exit /b 1
EOF

(cd /mnt/c && cmd.exe /c "${mirror_win}\\build.bat")

mkdir -p "$project_dir/out"
for f in ats_reverse_overlay.dll core_tests.exe; do
    if [ -f "$mirror/build-${config}/$f" ]; then
        cp "$mirror/build-${config}/$f" "$project_dir/out/"
    fi
done
echo "Build ${config} OK -> $project_dir/out"
