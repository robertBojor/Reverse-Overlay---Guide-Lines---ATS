#!/usr/bin/env bash
# Builds the plugin release zip: out/release/Backing_Lane_Prediction_v<VERSION>.zip
# One DLL for ATS and ETS2, plus each game's local mod package.
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

# Local (non-Workshop) mod packages, zipped with .NET (deflate) on Windows.
cp "$root/tools/pack_scs.ps1" "$mirror/"
pack() { # game stage-dir scs-name
    python3 -I "$root/tools/make_assets.py" scs "$1" >/dev/null
    rm -rf "${mirror:?}/$2"
    cp -r "$root/out/$2" "$mirror/$2"
    (cd /mnt/c && powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${mirror_win}\\pack_scs.ps1" \
        -Stage "${mirror_win}\\$2" -Destination "${mirror_win}\\$3" >/dev/null)
    cp "$mirror/$3" "$root/out/"
}
pack ats mod_stage ats_reverse_overlay.scs
pack ets2 mod_stage_ets2 backing_lane_prediction_ets2.scs

python3 -I - "$root" "$version" <<'EOF'
import sys, zipfile
from pathlib import Path

root, version = Path(sys.argv[1]), sys.argv[2]
out = root / 'out' / 'release'
out.mkdir(parents=True, exist_ok=True)
name = f'Backing_Lane_Prediction_v{version}'
target = out / f'{name}.zip'

def crlf(text):
    return text.replace('\r\n', '\n').replace('\n', '\r\n').encode()

install_bat = r'''@echo off
title Backing ^& Lane Prediction - install (ATS and ETS2)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1" -Dll "%~dp0ats_reverse_overlay.dll"
echo.
pause
'''
uninstall_bat = r'''@echo off
title Backing ^& Lane Prediction - uninstall (ATS and ETS2)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1" -Uninstall
echo.
pause
'''
readme = f'''Backing & Lane Prediction  v{version}
=====================================
For American Truck Simulator and Euro Truck Simulator 2.
(formerly "Reverse Overlay - Guide Lines - ATS")

Backup-camera style guide lines while reversing, and a forward line for
lane position. One plugin serves both games.

You need BOTH parts, for each game you play:

  1. The models: subscribe to the Steam Workshop item for your game and
     enable it in the Mod Manager:
       ATS:  "Backing & Lane Prediction - ATS"
       ETS2: "Backing & Lane Prediction - ETS2"
     (No Workshop? Copy the file for your game from the optional folder
     into the mod folder in your Documents, and enable that instead:
       ATS:  ats_reverse_overlay.scs
             -> Documents\\American Truck Simulator\\mod
       ETS2: backing_lane_prediction_ets2.scs
             -> Documents\\Euro Truck Simulator 2\\mod
     Never enable both the Workshop item and the local file.)

  2. This plugin: exit the game(s) and run Install.bat. It copies
     ats_reverse_overlay.dll into <game folder>\\bin\\win_x64\\plugins of
     every game it finds (the folder is created if needed).

Start the game. It will mention "advanced SDK features" - that is expected.
In a forward gear, a light blue line runs from the middle of the front axle
along the path the truck will take. In reverse, the guide lines show the
path of the rear corners.

KEYBOARD SHORTCUT: Ctrl+Shift+F9 (only while the game window has focus)
Each press moves to the next mode:
  1st press   forward line only
  2nd press   reverse lines only
  3rd press   both (forward line when driving, lines in reverse)
  4th press   nothing
...then back to forward line only. The choice is remembered between games
(the first start is on "both"). Another key can be set in the settings file.

Updating from 0.4.0? Just run Install.bat; the ATS mod is unchanged.
From 0.3.0 or older: let Steam update the ATS Workshop item too, because
this plugin needs its blue line model. Your settings file is kept.

Full instructions, settings and troubleshooting: User_Guide.md
(open it with any text editor or a Markdown viewer).

Settings, log and mode file are kept per game, in
  Documents\\American Truck Simulator  or  Documents\\Euro Truck Simulator 2:
    ats_reverse_overlay.ini, ats_reverse_overlay.log, ats_reverse_overlay.mode

Uninstall: run Uninstall.bat (removes the DLL from both games) and disable
the mod.

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
    'optional/backing_lane_prediction_ets2.scs': (root / 'out' / 'backing_lane_prediction_ets2.scs').read_bytes(),
}
for lic in sorted((root / 'third_party' / 'licenses').glob('*.txt')):
    files[f'LICENSES/{lic.name}'] = lic.read_bytes()
files['LICENSE.txt'] = crlf((root / 'LICENSE').read_text())

with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for fname in sorted(files):
        z.writestr(f'{name}/{fname}', files[fname])
print(f'wrote {target}')
for fname in sorted(files):
    print('  ', fname)
EOF
