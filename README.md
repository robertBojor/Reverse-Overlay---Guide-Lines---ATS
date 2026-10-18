<div align="center">

# Backing & Lane Prediction · ATS

**Backup-camera style guide lines for reversing in American Truck Simulator,
plus forward dots for lane position, drawn in the game world and visible in
the mirrors.**

![Game](https://img.shields.io/badge/game-American%20Truck%20Simulator%201.61-1f6feb)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-555)
![Version](https://img.shields.io/badge/version-0.2.0-2ea44f)
![Status](https://img.shields.io/badge/status-early%20release-orange)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Steam Workshop](https://img.shields.io/badge/Steam%20Workshop-subscribe-171a21?logo=steam)](https://steamcommunity.com/sharedfiles/filedetails/?id=3822108979)

</div>

*Formerly **Reverse Overlay - Guide Lines - ATS**. Same Workshop item, same plugin file, new name for 0.2.0.*

<p align="center"><img src="assets/images/preview-image-large.jpg" width="820" alt="Backing &amp; Lane Prediction: on the left, red, yellow and green reverse guide lines behind a truck and trailer at night; on the right, a red and a green dot on the highway ahead of a truck, marking where its front corners will be"></p>

When you shift into reverse, two lines appear on the ground from the rear
corners of your trailer, or of the truck when you drive without one. They follow
the path the rig will actually take if you hold the current steering:

| | |
|---|---|
| 🟥 **0 – 3 m** | red |
| 🟨 **3 – 8 m** | yellow |
| 🟩 **8 – 15 m** | green, with ticks where each band ends |

<p align="center"><img src="assets/images/screenshot-cab-mirror.jpg" width="820" alt="Cab view while reversing: the red, yellow and green guide lines are visible in the side mirror"></p>

In a **forward gear**, two square dots appear ahead of the truck where its
front corners will be about 1.5 seconds from now (at least 6 m ahead): 🟥 red
for the front-left corner, 🟩 green for the front-right. Keep red inside the
left lane line and green inside the right one.

## Keyboard shortcut: Ctrl+Shift+F9

One shortcut chooses what you see. Each press moves to the next mode:

| Press | Shows |
|---|---|
| 1st | **Forward dots only** |
| 2nd | **Reverse lines only** |
| 3rd | **Both**: dots in a forward gear, lines in reverse |
| 4th | **Nothing** (everything hidden) |

- The next press starts again with forward dots only.
- The mode is **remembered** between game sessions. The very first start
  is on **Both**.
- The shortcut works only while the game window has focus.
- The log confirms each change, e.g. *"Hotkey: showing forward dots only."*
- To use a different key, edit `[hotkey]` in the settings file
  ([details](docs/USER_GUIDE.md#hotkey)).

## What's new in 0.2.0

- **Forward dots** for lane position in any forward gear.
- **Ctrl+Shift+F9** now cycles forward only / reverse only / both / off, and
  remembers the choice.
- The optional top-down panel was removed. The plugin no longer hooks DirectX
  at all.

Full history: [CHANGELOG.md](CHANGELOG.md).

## Features

- **In the world, not on the screen:** the lines are real objects, so they show
  up from every camera **and in the mirrors**, where you actually reverse a
  trailer.
- **Real steering geometry:** the path is computed from the game's actual
  front-wheel angles, so it handles different steering locks, lift axles and
  multi-steer trucks.
- **Trailers done properly:** single trailers, doubles and triples, off-axle
  fifth wheels and self-steering trailer axles. Couplings are measured live.
- **Patch-tolerant:** game functions are found by code patterns, not fixed
  addresses. If an update breaks them, the lines switch off cleanly instead of
  crashing the game, and a bundled tool regenerates the patterns.
- **Forward dots:** the same steering model run forward for the truck, with a
  time-based look-ahead and smoothing so the dots stay steady at speed.
- **Lightweight:** about 140 small line pieces while reversing, 2 small tiles
  while driving forward. No network access, and save games are never touched.
- **Configurable:** band distances, path length, look-ahead, dot size and
  heights, all changeable while you drive.

## Installation

You need **two parts**. Steam Workshop can't install plugins, so the DLL is a
separate download.

1. **Line models:** subscribe to [**Backing & Lane Prediction - ATS**](https://steamcommunity.com/sharedfiles/filedetails/?id=3822108979) on the
   Steam Workshop, *or* use the `.scs` included in the download.
2. **Plugin:** download `Backing_Lane_Prediction_ATS_v0.2.0.zip` from the
   [**latest release**](../../releases/latest), exit the game and run `Install.bat`. It
   copies `ats_reverse_overlay.dll` into
   `American Truck Simulator\bin\win_x64\plugins`.
3. Start ATS, accept the *advanced SDK features* notice, enable the mod in the
   Mod Manager, and drive. Press **Ctrl+Shift+F9** to choose what is shown
   ([see above](#keyboard-shortcut-ctrlshiftf9)).

> [!IMPORTANT]
> **Updating from 0.1.0:** update both parts. The plugin needs the new dot
> models from the Workshop item (Steam updates it on its own) or the new
> `.scs`. Until then the reverse lines still work, the dots are missing, and
> the log reports *"Could not create /model/rto_guides/dot_red.pmd"*.

The **[user guide](docs/USER_GUIDE.md)** covers settings, troubleshooting, game
updates and uninstalling.

> [!NOTE]
> The plugin is not code-signed yet. Windows Smart App Control or antivirus
> software may block or flag it. See
> [Windows security](docs/USER_GUIDE.md#7-windows-security-unsigned-plugin).

## How it works

```mermaid
flowchart LR
    T["SCS Telemetry SDK<br/>gear · speed · wheels · steering · placement · hitches"] --> P["Prediction<br/>src/core/rig.hpp"]
    P -->|reverse| G["Guide lines<br/>src/core/guides.hpp"]
    P -->|forward| D["Forward dots<br/>src/core/dots.hpp"]
    G --> W["World renderer<br/>src/plugin/world_lines.cpp"]
    D --> W
    E["Pattern scan<br/>src/plugin/engine.cpp"] --> W
    W --> R["Game render pass<br/>(all cameras + mirrors)"]
```

- **Prediction:** a kinematic bicycle model about the tractor's virtual rear
  axle, with curvature from the real wheel angles,
  `k = tan(δ) / (L − x·tan(δ))`. That's exact for Ackermann steering. Each
  trailer obeys a no-side-slip constraint at its equivalent axle, driven by the
  movement of its coupling point. Integration stops at 85° articulation.
- **Guide lines:** the rear-corner paths of the last unit, cut into 0.25 m
  pieces and coloured by distance travelled.
- **Forward dots:** the tractor's pose after `clamp(speed × lookahead_s,
  min, max)` metres, solved as an exact arc, with the curvature smoothed over
  0.25 s. Each dot is centred on a predicted front corner. Model instances
  can't be scaled, so bigger dots are a grid of 12 cm tiles. Trailers are
  ignored.
- **Rendering:** plugin-owned model instances submitted from the player
  vehicle's render pass. Five engine functions are located by unique
  wildcarded byte patterns. A function another plugin has already hooked is
  still recognised.

## Game updates

After an update, the log shows either *"engine functions located"* (all good)
or *"engine functions missing"* (lines off, game unaffected). In the second
case:

```bash
tools/re/update.sh --apply   # find the functions in the new build, rewrite the patterns
tools/build.sh Release       # rebuild the plugin
```

The finder compares the new executable with a known-good reference using four
independent methods:
- exact patterns;
- loose patterns;
- instruction-shape fingerprints over every function;
- caller voting.

It accepts a result only when they agree. See
[Game updates](docs/USER_GUIDE.md#5-game-updates) for the player side.

## Building from source

Requirements: Windows with **Visual Studio 2026 Build Tools** (MSVC, CMake,
Ninja). The scripts are written for WSL, but the CMake project builds on
Windows directly.

```bash
tools/build.sh Release                  # plugin DLL -> out/
python3 -I tools/make_assets.py all     # mod files: out/mod_stage + workshop/ats/upload
tools/make_release.sh                   # release zip -> out/release/
```

Core tests (platform-neutral, any C++20 compiler):

```bash
c++ -std=c++20 -O2 tests/core_tests.cpp -o core_tests && ./core_tests
```

Workshop publishing steps: [workshop/ats/UPLOAD.md](workshop/ats/UPLOAD.md).

<details>
<summary><b>Repository layout</b></summary>

```
src/core/       platform-neutral prediction, guide lines, forward dots, hotkey modes (unit tested)
src/plugin/     SDK plugin, pattern scan, world renderer, settings, log
assets/base/    base line model the coloured variants are generated from
tests/          core unit tests
tools/          build, asset, packaging and install scripts
tools/re/       pattern generation and the game-update finder
workshop/ats/   Steam Workshop upload folder, description and checklist
docs/           user guide
third_party/    SCS SDK headers, MinHook, licences
```
</details>

## Roadmap

- [ ] Euro Truck Simulator 2 version
- [ ] Code-signed releases
- [ ] Better height on uneven ground (wheel-based ground fit)
- [ ] Configurable colours

## Credits

- In-world drawing technique and base line model:
  [Reverse Posture Assistant For ATS](https://github.com/yyysheng/ATSmods) by
  yyysheng (MIT).
- SCS Telemetry SDK
  · [MinHook](https://github.com/TsudaKageyu/minhook) (BSD-2-Clause).

## License

[MIT](LICENSE) © 2026 Robert Bojor. Third-party components keep their own
licences; see [third_party/licenses](third_party/licenses).

*Not affiliated with or endorsed by SCS Software. American Truck Simulator is a
trademark of SCS Software.*
