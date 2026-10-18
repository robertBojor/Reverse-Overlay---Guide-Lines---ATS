#!/usr/bin/env bash
# Builds the plugin release zip: out/release/Backing_Lane_Prediction_ATS_v<VERSION>.zip
#   tools/make_release.sh            # uses the DLL already in out/
#   tools/make_release.sh --build    # rebuild the DLL first
# The DLL should be signed before running this for a public release.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
version="$(tr -d '[:space:]' < "$root/VERSION")"
win_user="$(cmd.exe /c 'echo %USERNAME%' 2>/dev/null | tr -d '\r')"
mirror="/mnt/c/Users/${win_user}/ats-reverse-overlay-build"
mirror_win="C:\\Users\\${win_user}\\ats-reverse-overlay-build"

if [ "${1:-}" = "--build" ]; then
    "$root/tools/build.sh" Release
fi
dll="$root/out/ats_reverse_overlay.dll"
[ -f "$dll" ] || { echo "Missing $dll - run tools/build.sh Release" >&2; exit 1; }

# Local (non-Workshop) mod package, zipped with .NET (deflate) on Windows.
python3 -I "$root/tools/make_assets.py" scs >/dev/null
rm -rf "$mirror/mod_stage"
cp -r "$root/out/mod_stage" "$mirror/mod_stage"
cp "$root/tools/pack_scs.ps1" "$mirror/"
(cd /mnt/c && powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${mirror_win}\\pack_scs.ps1" \
    -Stage "${mirror_win}\\mod_stage" -Destination "${mirror_win}\\ats_reverse_overlay.scs" >/dev/null)
cp "$mirror/ats_reverse_overlay.scs" "$root/out/"

python3 -I - "$root" "$version" <<'EOF'
import sys, zipfile
from pathlib import Path

root, version = Path(sys.argv[1]), sys.argv[2]
out = root / 'out' / 'release'
out.mkdir(parents=True, exist_ok=True)
target = out / f'Backing_Lane_Prediction_ATS_v{version}.zip'

def crlf(text):
    return text.replace('\r\n', '\n').replace('\n', '\r\n').encode()

install_bat = r'''@echo off
title Backing ^& Lane Prediction - ATS - install
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1" -Dll "%~dp0ats_reverse_overlay.dll"
echo.
pause
'''
uninstall_bat = r'''@echo off
title Backing ^& Lane Prediction - ATS - uninstall
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1" -Uninstall
echo.
pause
'''
readme = f'''Backing & Lane Prediction - ATS  v{version}
==========================================
(formerly "Reverse Overlay - Guide Lines - ATS")

Backup-camera style guide lines while reversing in American Truck Simulator,
and forward dots for lane position.

You need BOTH parts:

  1. The models: subscribe to "Backing & Lane Prediction - ATS" on
     the Steam Workshop and enable it in the Mod Manager.
     (No Workshop? Copy optional\\ats_reverse_overlay.scs into
     Documents\\American Truck Simulator\\mod and enable that instead.
     Never enable both.)

  2. This plugin: exit the game and run Install.bat. It copies
     ats_reverse_overlay.dll into
     <ATS folder>\\bin\\win_x64\\plugins  (the folder is created if needed).

Start the game. It will mention "advanced SDK features" - that is expected.
In a forward gear, two dots show where the truck's front corners will be
(red = front-left, green = front-right); with speed they stretch into lines
along the path those corners will take. In reverse, the guide lines show
the path of the rear corners.

KEYBOARD SHORTCUT: Ctrl+Shift+F9 (only while the game window has focus)
Each press moves to the next mode:
  1st press   forward dots only
  2nd press   reverse lines only
  3rd press   both (dots forward, lines in reverse)
  4th press   nothing
...then back to forward dots only. The choice is remembered between games
(the first start is on "both"). Another key can be set in the settings file.

Updating from 0.2.0? Only the plugin changes; the mod stays as it is. Your
settings file keeps the old dot distance and size: set min_distance_m = 14
and dot_size_m = 0.5 under [forward], or delete the file for fresh defaults.

Updating from 0.1.0? Update the mod as well as the plugin: the plugin needs
the dot models.

Full instructions, settings and troubleshooting: User_Guide.md
(open it with any text editor or a Markdown viewer).

Settings:  Documents\\American Truck Simulator\\ats_reverse_overlay.ini
Log:       Documents\\American Truck Simulator\\ats_reverse_overlay.log
Mode:      Documents\\American Truck Simulator\\ats_reverse_overlay.mode

Uninstall: run Uninstall.bat (removes the DLL) and disable the mod.

After a game update the lines may switch themselves off until the plugin is
updated (the log then says "engine functions missing"). The game itself is
not affected.

Do not use together with "Reverse Posture Assistant" (it hooks the same
part of the game).

Credits: in-world drawing technique and base line model from
"Reverse Posture Assistant For ATS" by yyysheng (MIT). Uses the SCS
Telemetry SDK and MinHook - licences in the LICENSES folder.
'''

files = {
    'ats_reverse_overlay.dll': (root / 'out' / 'ats_reverse_overlay.dll').read_bytes(),
    'Install.bat': crlf(install_bat),
    'Uninstall.bat': crlf(uninstall_bat),
    'README.txt': crlf(readme),
    'User_Guide.md': crlf((root / 'docs' / 'USER_GUIDE.md').read_text()),
    'installer/install.ps1': crlf((root / 'tools' / 'install.ps1').read_text()),
    'optional/ats_reverse_overlay.scs': (root / 'out' / 'ats_reverse_overlay.scs').read_bytes(),
}
for lic in sorted((root / 'third_party' / 'licenses').glob('*.txt')):
    files[f'LICENSES/{lic.name}'] = lic.read_bytes()
files['LICENSE.txt'] = crlf((root / 'LICENSE').read_text())

with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for name in sorted(files):
        z.writestr(f'Backing_Lane_Prediction_ATS_v{version}/{name}', files[name])
print(f'wrote {target}')
for name in sorted(files):
    print('  ', name)
EOF
