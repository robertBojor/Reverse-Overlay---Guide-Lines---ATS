# Backing & Lane Prediction · User guide

*For American Truck Simulator and Euro Truck Simulator 2. Formerly "Reverse
Overlay - Guide Lines - ATS"; the ATS Workshop item and the plugin file
(`ats_reverse_overlay.dll`) are the same, only the name changed.*

Backup-camera style guide lines for American Truck Simulator and Euro Truck
Simulator 2. When you shift
into reverse, two lines appear on the ground behind your trailer (or behind the
truck when you drive without a trailer). They show where the rear corners will
go if you keep the steering where it is.

- **Red** for the first 3 m, **yellow** to 8 m, **green** out to 15 m.
- Short **ticks** point inwards where the colour changes and at the end of the
  path.
- The lines are real objects in the game world, so you see them from **every
  camera and in the mirrors**.
- They follow your **actual front-wheel angles**, so different steering locks,
  lift axles and multi-steer trucks are handled.
- They work with one trailer and with doubles and triples. The lines start
  behind the last trailer.

In a **forward gear**, a light **blue line** runs on the road from the middle
of the truck's front axle along the path it will take. It helps you keep the
truck centred in its lane. **Ctrl+Shift+F9** chooses whether you see the
forward line, the reverse lines, both or neither.

> **Keyboard shortcut: Ctrl+Shift+F9**
> Each press moves to the next mode: **forward line only → reverse lines only
> → both → nothing**, then back to forward line only. Your choice is
> remembered. Details in [section 3](#what-is-shown-ctrlshiftf9).

**New in 0.5.0:** Euro Truck Simulator 2. One plugin serves both games, and
`Install.bat` installs it into every game it finds. ETS2 has its own Workshop
item. ATS players only need the new plugin if they want the new installer;
nothing changes in ATS itself. See the [changelog](../CHANGELOG.md).

**Paths in this guide:** *the game's Documents folder* means
`Documents\American Truck Simulator` or `Documents\Euro Truck Simulator 2`,
and *the game folder* means where Steam installed the game, for example
`...\steamapps\common\Euro Truck Simulator 2`. Settings, log and mode file
are separate for each game.

---

## 1. Requirements

- American Truck Simulator **1.61** or Euro Truck Simulator 2 **1.61**, on
  **Windows 64-bit** (Steam).
- Tested with the DirectX 11 renderer. The guide lines use the game's own
  rendering, so the renderer shouldn't matter, but the others are untested.
- Not available for macOS or Linux.

---

## 2. Installation

The mod comes in **two parts**, and you need both:

| Part | What it does | Where it comes from |
|---|---|---|
| **Models** | The coloured line pieces the game draws | The Steam Workshop item for your game *or* the `.scs` in the download's `optional` folder |
| **Plugin** (`ats_reverse_overlay.dll`) | Predicts the path and places the lines | The plugin download (zip) |

Steam Workshop can only deliver mod files. It cannot install the plugin, so
**subscribing on its own does nothing**.

### Step 1: Get the models (choose one)

**A. Steam Workshop (recommended)**
1. Subscribe to the item for your game:
   - ATS: [**Backing & Lane Prediction - ATS**](https://steamcommunity.com/sharedfiles/filedetails/?id=3822108979)
   - ETS2: [**Backing & Lane Prediction - ETS2**](https://steamcommunity.com/sharedfiles/filedetails/?id=3823038411)
2. Wait until Steam has downloaded it.

**B. Without the Workshop**
1. From the plugin download's `optional` folder, copy the file for your game
   into the `mod` folder in the game's Documents folder:
   - ATS: `ats_reverse_overlay.scs` → `Documents\American Truck Simulator\mod\`
   - ETS2: `backing_lane_prediction_ets2.scs` → `Documents\Euro Truck Simulator 2\mod\`

Use **A or B, not both**. Playing both games? Do this for each one.

### Step 2: Install the plugin

1. **Exit the game completely.**
2. Download `Backing_Lane_Prediction_v<version>.zip` from the
   [latest release](https://github.com/robertBojor/Reverse-Overlay---Guide-Lines---ATS/releases/latest).
3. Before extracting, right-click the zip → **Properties**, tick **Unblock**
   if it's there, then click **OK**. This avoids extra Windows security prompts
   (see [section 7](#7-windows-security-unsigned-plugin)).
4. Extract the zip anywhere.
5. Double-click **`Install.bat`**.
   - It finds ATS and ETS2 through Steam and installs into **every game it
     finds**: it copies `ats_reverse_overlay.dll` into the game folder's
     `bin\win_x64\plugins\` and creates the `plugins` folder if needed.
   - It prints one **installed …** line per game; then you can close the
     window.
   - To install into one game only, run it from a command prompt as
     `powershell -ExecutionPolicy Bypass -File installer\install.ps1 -Game ets2`
     (or `-Game ats`).

To install by hand instead, copy `ats_reverse_overlay.dll` into
`<game folder>\bin\win_x64\plugins\`. Create the `plugins` folder if it's
missing. The game only loads plugins from that exact folder.

### Step 3: Enable the mod

1. Start the game.
2. The game shows a notice that a plugin uses **advanced SDK features**. This is
   expected for any telemetry plugin. Confirm it.
3. Open the **Mod Manager**, enable **Backing & Lane Prediction - ATS** (or
   **- ETS2**), and confirm the changes.
4. Load your profile and drive: the blue line appears in a forward gear, the
   guide lines in reverse. Press **Ctrl+Shift+F9** to choose what is shown.

### Check it's working

Open `ats_reverse_overlay.log` in the game's Documents folder. After
starting the game you should see:

```
Backing & Lane Prediction 0.5.0 starting in Euro Truck Simulator 2 ...
Showing forward line and reverse guide lines (the hotkey changes this).
engine functions located: ...
In-world guide lines ready (vehicle render hook installed).
```

The first time something is drawn:

```
In-world guide lines rendering: 50 pieces.
```

(The number varies: 50 to 80 for the forward line, about 140 while
reversing.) If
these lines are missing, see [Troubleshooting](#6-troubleshooting).

---

## 3. Using it

### What is shown: Ctrl+Shift+F9

| Press | Shows |
|---|---|
| 1st | **Forward line only** |
| 2nd | **Reverse lines only** |
| 3rd | **Both**: the forward line in a forward gear, guide lines in reverse |
| 4th | **Nothing** (everything hidden) |

- The next press starts again with the forward line only.
- The mode is **remembered** between game sessions, in
  `ats_reverse_overlay.mode` in the game's Documents folder. The very
  first start is on **Both**. To reset, delete that file.
- The shortcut works only while the **game window has focus**. It does nothing
  in the Windows desktop or another program.
- The log records each change, for example *"Hotkey: showing forward line
  only."*
- To use another key combination, see [`[hotkey]`](#hotkey).

### Forward line

- It appears in **any forward gear**, even when you're stopped, and
  disappears in neutral and while the game is paused or in menus.
- It starts under the **middle of the front axle** and follows the path the
  truck will take if you hold the steering, so on a bend its curve starts
  where the truck is. Its first metres are hidden under the hood.
- **Length:** 25 m when slow or stopped. From about 75 km/h (47 mph) it grows
  with speed, 1.2 seconds of travel, up to 40 m at 120 km/h (75 mph). On a
  tight turn it stops after a quarter turn instead of curling round.
- **At speed it follows how the truck is really turning.** Up to about
  18 km/h (11 mph) the curve comes from the front-wheel angles. Above about
  36 km/h (22 mph) it comes from the truck's measured turn rate, because at
  speed the tyres slip and the wheel angle alone would over-bend the line.
- Steering is **smoothed** a little so the line doesn't jump with every small
  correction. It catches up within about half a second.
- It is light blue, 0.3 m wide and **see-through** (75 % opacity), and glows
  softly at night.
- Only the truck counts. A trailer follows inside the truck's path on curves.
- **Hills:** the game doesn't tell the plugin where the road surface is ahead,
  so the line continues the slope the truck is on now. Before a crest its far
  end can float; where the road starts to climb it can sink into the road.

### Reverse guide lines

- The lines appear when the **gearbox is in reverse** and disappear when you
  shift out. They are hidden while the game is paused or in menus.
- The path assumes you **hold the current steering**. Turn the wheel and
  the lines update immediately. Use them to find the steering angle that puts
  the trailer where you want it, then follow that.
- **Distance bands** measure how far the rig has to travel to reach that
  point, not the straight-line distance.
- Lines are drawn about **18 cm outside** the rear corners of the last unit,
  as a safety margin.

### Accuracy notes

- The game doesn't report trailer or truck body sizes. **Width and rear
  overhang are estimated** from the wheel and kingpin positions. With trailers
  that have an unusually long tail, treat the lines as approximate.
- On slopes, the lines follow the tilt of the rear unit. On uneven ground,
  such as dock ramps or kerbs, they may float slightly or dip into the surface.
  You can raise or lower them with `line_height_m`
  (see [Settings](#4-settings)).
- If the prediction reaches a **jackknife** (about 85° between truck and
  trailer), the path stops there.

---

## 4. Settings

File: `ats_reverse_overlay.ini` in the game's Documents folder, one per game.
It's created on the first game start. Edit it with any text editor. Most
changes apply **while you drive** (within about 2 seconds).

### `[world]` guide lines

| Setting | Default | Meaning |
|---|---|---|
| `enabled` | `1` | `0` turns off everything drawn in the world (reverse and forward lines) |
| `red_until_m` | `3` | end of the red band, in metres of travel |
| `yellow_until_m` | `8` | end of the yellow band; green runs from here to the end |
| `line_height_m` | `0.35` | height of the lines; lower it if they float, raise it if they sink into the road |
| `allow_unverified` | `1` | `1`: also run on game versions not yet confirmed. `0`: only on confirmed versions (see [section 5](#5-game-updates)) |

### `[prediction]`

| Setting | Default | Meaning |
|---|---|---|
| `horizon_m` | `15` | how far ahead (backwards) the path is predicted, 3–60 m |

### `[forward]` forward line

| Setting | Default | Meaning |
|---|---|---|
| `min_length_m` | `25` | length when slow or stopped, from the front axle (the first part is under the truck), 5–100 m |
| `length_time_s` | `1.2` | with speed the line is this many seconds of travel long, 0–5 |
| `max_length_m` | `40` | and never longer than this, 5–100 m |
| `width_m` | `0.3` | line width, in steps of 0.15 m, 0.15–0.6 m |
| `height_m` | `0.35` | height of the line; lower it if it floats, raise it if it sinks into the road |
| `steering_smoothing_s` | `0.25` | how calmly the line follows the steering, in seconds; `0` = no smoothing |

Settings files from 0.2.0 and 0.3.0 contain the forward dot settings
(`lookahead_s`, `min_distance_m`, `dot_size_m` and so on). They are ignored;
`dot_height_m` still sets the height. Delete the file to get a fresh commented
copy.

### `[hotkey]`

**Ctrl+Shift+F9** steps through forward line only, reverse lines only, both,
and nothing (see [Using it](#3-using-it)). The current mode is saved in
`ats_reverse_overlay.mode` next to the settings file.

| Setting | Default | Meaning |
|---|---|---|
| `key` | `F9` | a letter, a digit, `F1`–`F12`, or `none` to turn the hotkey off |
| `ctrl`, `shift`, `alt` | `1`, `1`, `0` | which modifier keys must be held |

The key only works while the game window has focus. Example: Alt+F10 instead:

```ini
[hotkey]
key = F10
ctrl = 0
shift = 0
alt = 1
```

Avoid combinations the game already uses (Ctrl+Shift+R, for example, opens
the radio). Changes apply within about 2 seconds.

Older versions also had an `[overlay]` section for a top-down panel. That
panel has been removed; an `[overlay]` section left in your file is ignored.

---

## 5. Game updates

The plugin finds the game functions it needs by searching for code patterns,
not fixed addresses, so small game updates often don't affect it. After an
update, one of three things happens:

| What you see | Meaning | What to do |
|---|---|---|
| Lines work. The log says *"build not yet verified in game"* | The patterns still match | Nothing |
| No lines. The log says *"engine functions missing"* and *"In-world guide lines unavailable on this game build"* | The update changed the code the plugin uses. The lines switch themselves off, and the game runs normally | Wait for a plugin update, or roll back the game (below) |
| Problems or crashes when reversing right after an update | Rare: the code still matched but works differently | Set `allow_unverified = 0`, or remove the DLL, until the plugin is updated |

**Rolling back the game:** in Steam, right-click the game →
**Properties → Betas** and pick the previous version, if SCS offers it.

---

## 6. Troubleshooting

**Start with the log:** `ats_reverse_overlay.log` in the game's Documents
folder. The game's own log is `game.log.txt` in the same folder; search
it for `lane-prediction` or `ats_reverse_overlay`.

| Symptom | Likely cause | Fix |
|---|---|---|
| No `ats_reverse_overlay.log` at all, or it isn't updated | The plugin isn't loaded | Check that the DLL is in `bin\win_x64\plugins\` (not just `bin\win_x64\`). In `game.log.txt`, look for `loading 'ats_reverse_overlay'`. If the game log shows the plugin failing to load, see [section 7](#7-windows-security-unsigned-plugin) |
| Log: *"Could not create /model/rto_guides/… - is the … mod enabled in the Mod Manager?"* | The line models aren't loaded | Enable **Backing & Lane Prediction - ATS** (or **- ETS2**) in the Mod Manager. Then restart the game, because a model that failed to load stays off for the session |
| Log: *"engine functions missing"* | Unsupported game version | See [Game updates](#5-game-updates) |
| Log: *"In-world guide lines skipped: build not verified and allow_unverified = 0"* | Safety mode is on | Set `allow_unverified = 1` if you want to try it on this version |
| Log: *"In-world rendering raised an exception…"* | Something went wrong while drawing. The lines are switched off for the session | Restart the game. If it repeats, send the log with your report |
| Lines float above or sink into the road | Height estimate | Adjust `line_height_m` |
| Lines look too narrow or wide for your trailer | Body size is estimated from the wheels | Known limitation, see [Accuracy notes](#accuracy-notes) |
| Two sets of lines (blue/orange and red/yellow/green) | "Reverse Posture Assistant" is also installed | Use one or the other. Remove `ATSReverseTrajectoryRuntime.dll` from `plugins` and disable that mod |
| Lines don't appear, but the log shows no errors | The hotkey mode doesn't include them | Press **Ctrl+Shift+F9** until the log says *"Hotkey: showing forward line and reverse guide lines."* The mode is remembered across restarts |
| The far end of the blue line sinks into the road or floats | The road climbs or drops ahead (see [Forward line](#forward-line)) | Expected on hills. A lower `max_length_m` keeps it closer to the truck |
| Log: *"Could not create /model/rto_guides/line_blue.pmd"* | The mod files are older than the plugin. The reverse lines keep working; only the forward line is missing | Update the Workshop item (Steam usually does this on its own) or the local `.scs`, then restart the game |
| Ctrl+Shift+F9 does nothing | The game window doesn't have focus, or `[hotkey]` was changed | Click into the game first. Check `key`, `ctrl`, `shift` and `alt` in the settings file |

---

## 7. Windows security (unsigned plugin)

The plugin DLL is **not digitally signed** yet. Windows and antivirus software
treat unsigned files from the internet with suspicion, so you may run into the
following:

- **"Windows protected your PC" or "Open File – Security Warning"** when you
  run `Install.bat`. Unblocking the zip before extracting (Step 2) usually
  prevents this. Otherwise choose **More info → Run anyway**, or copy the DLL
  by hand.
- **Smart App Control** (Windows 11: Windows Security → App & browser control).
  When it's **On**, Windows may refuse to load unsigned DLLs it doesn't
  recognise. The game then starts normally, but the plugin isn't loaded and no
  `ats_reverse_overlay.log` appears. There's no safe workaround for one file
  other than turning Smart App Control off. Depending on your Windows version,
  turning it back on may require resetting Windows, so decide carefully.
- **Antivirus false positives.** To draw in the world, the plugin hooks into the
  game's rendering, a technique antivirus software sometimes flags. If yours
  quarantines the DLL, you can add an exception for the `plugins` folder.

What the plugin does and doesn't do:
- It runs only inside ATS or ETS2, loaded by the game's official plugin
  system.
- It reads telemetry, and it only writes `ats_reverse_overlay.log`,
  `ats_reverse_overlay.ini` and `ats_reverse_overlay.mode` in the game's
  Documents folder.
- It has **no network access** and doesn't touch your save games.

---

## 8. Uninstall

1. Exit the game.
2. Run **`Uninstall.bat`** from the download (it removes the plugin from
   both games), or delete
   `<game folder>\bin\win_x64\plugins\ats_reverse_overlay.dll`.
3. Disable or unsubscribe **Backing & Lane Prediction - ATS** / **- ETS2** (or
   delete its `.scs` from the `mod` folder in the game's Documents folder).
4. Optionally delete `ats_reverse_overlay.ini`, `ats_reverse_overlay.log` and
   `ats_reverse_overlay.mode` from the game's Documents folder.

---

## 9. FAQ

**Does it work in Convoy (multiplayer)?** Untested.

**Euro Truck Simulator 2?** Yes, since 0.5.0: the same plugin, and its own
Workshop item. Install the plugin once; `Install.bat` puts it into both games.

**Mac or Linux?** Not currently.

**Does it affect performance?** Barely. It's about 140 small line pieces while
reversing and at most 80 while driving forward.

**Can I change the colours?** Not yet. The band distances can be changed.

---

## 10. Credits and licences

- In-world drawing technique and base line model: *Reverse Posture Assistant
  For ATS* by yyysheng (MIT licence).
- SCS Telemetry SDK © SCS Software. MinHook © Tsuda Kageyu (BSD-2-Clause).
- Licence texts are in the `LICENSES` folder of the download.
