# Reverse Overlay - Guide Lines - ATS

Backup-camera style guide lines for American Truck Simulator. While you are
in reverse, two lines are drawn on the ground from the rear corners of your
trailer (or truck, when bobtail) along the predicted path: red for the first
3 m, yellow to 8 m, then green out to 15 m, with ticks where the colour
changes. They are real objects in the game world, so they show up in every
camera and in the mirrors.

## How it works

- **Prediction** uses only the official SCS telemetry SDK: truck and trailer
  placement, wheel layout, per-wheel steering, lift and ground contact, and
  hitch positions.
- **Drawing** places small line models (from the
  `Reverse Overlay - Guide Lines - ATS` mod package) in the player vehicle's
  render pass. The five engine functions involved are found by wildcarded
  byte patterns, not by fixed address or executable hash. Each pattern must
  match exactly once, and a function that another plugin has already hooked
  is still recognised.
- **If a game update changes the code,** the patterns stop matching and the
  lines switch themselves off cleanly. To recover, regenerate the patterns
  for the new build with `tools/re/` (see "Game updates" below).

## Install

1. Exit ATS.
2. Copy `out/ats_reverse_overlay.dll` to `<ATS>\bin\win_x64\plugins\`, or run:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\install.ps1 -Dll out\ats_reverse_overlay.dll
   ```
3. Copy `out/ats_reverse_overlay.scs` to `Documents\American Truck Simulator\mod\`.
4. Start ATS, accept the "SDK features" prompt, and enable
   **Reverse Overlay - Guide Lines - ATS** in the Mod Manager.
5. Shift into reverse.

## Settings

`Documents\American Truck Simulator\ats_reverse_overlay.ini` is created on
first start and re-read while the game runs.

| Section / setting | Default | Meaning |
|---|---|---|
| `[world] enabled` | `1` | guide lines on or off |
| `[world] red_until_m`, `yellow_until_m` | `3`, `8` | colour band limits (metres travelled) |
| `[world] line_height_m` | `0.35` | line height above the rear unit's reference point |
| `[world] allow_unverified` | `1` | also run on game builds not yet confirmed in game; `0` = panel-only safety mode after updates |
| `[prediction] horizon_m` | `15` | prediction distance |
| `[overlay] enabled` | `0` | optional top-down panel (DirectX 11). Read at game start; when off, DirectX is not hooked |

The log is written to `Documents\American Truck Simulator\ats_reverse_overlay.log`.
Its startup lines show whether the engine functions were found and whether
the lines are ready.

## Game updates

1. After an update, start the game and check the log:
   - *"engine functions located … (build not yet verified in game)"*: the
     patterns still match. Test-drive, then add the new exe hash to
     `verified_hashes` in `src/plugin/engine.cpp`.
   - *"engine functions missing"*: the lines are off. Find the functions in
     the new `amtrucks.exe`, regenerate the patterns with
     `tools/re/make_patterns.py`, update `src/plugin/engine.cpp`, and rebuild.
2. If an update misbehaves rather than failing cleanly, set
   `allow_unverified = 0`, or switch back to the previous game version via
   Steam → Properties → Betas.

## How the prediction works

`src/core/rig.hpp` (platform neutral, unit tested):

- **Tractor:** kinematic bicycle model about the virtual rear axle, which is
  the centroid of the grounded rear axles, so lifting a tag axle shortens the
  wheelbase. Curvature comes from the **actual front-wheel angles** using
  `k = tan(d) / (L - x tan(d))` per wheel, which is exact for Ackermann
  steering.
- **Trailers:** no-side-slip constraint at the equivalent axle, driven by the
  displacement of the coupling point. Self-steering trailer axles are
  included.
- **Doubles and triples:** each coupling point is measured live from the
  telemetry placements.
- **Limits:** the SDK reports no body dimensions. The width and rear
  overhang of the rearmost unit are estimated from wheel and kingpin
  positions (`rto::tuning`). Integration stops at 85° articulation.

`src/core/guides.hpp` turns the prediction into 0.25 m line pieces with
colour bands and ticks.

## Layout

```
src/core/      rig.hpp (prediction), guides.hpp (guide lines), scene.hpp + panel_layout.hpp (panel)
src/plugin/    plugin.cpp (SDK), engine.cpp (pattern scan), world_lines.cpp (in-world lines),
               overlay_dx11.cpp (optional panel), config, log
assets/base/   line model the variants are derived from (MIT, see third_party/licenses)
tests/         core_tests.cpp, scene_preview.cpp
tools/         build.sh (WSL -> MSVC), make_assets.py + pack_scs.ps1 (mod package),
               install.ps1, re/ (pattern generation)
```

## Build

From WSL, using the Windows Visual Studio 2026 Build Tools:

```bash
tools/build.sh Release                 # DLL -> out/
python3 -I tools/make_assets.py        # stage the mod files in out/mod_stage
```

Then zip the staged files with `tools/pack_scs.ps1`. The archive must use
deflate compression: the game rejects stored zip entries.

## Credits

The in-world drawing technique and the base line model come from
"Reverse Posture Assistant For ATS" by yyysheng (MIT).
