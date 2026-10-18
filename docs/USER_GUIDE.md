# Reverse Overlay - Guide Lines - ATS · User guide

Backup-camera style guide lines for American Truck Simulator. When you shift
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

---

## 1. Requirements

- American Truck Simulator **1.61** on **Windows 64-bit** (Steam).
- Tested with the DirectX 11 renderer. The guide lines use the game's own
  rendering, so the renderer shouldn't matter, but the others are untested.
- Not available for macOS or Linux.

---

## 2. Installation

The mod comes in **two parts**, and you need both:

| Part | What it does | Where it comes from |
|---|---|---|
| **Guide-line models** | The coloured line pieces the game draws | Steam Workshop item *or* `optional\ats_reverse_overlay.scs` in the download |
| **Plugin** (`ats_reverse_overlay.dll`) | Predicts the path and places the lines while you reverse | The plugin download (zip) |

Steam Workshop can only deliver mod files. It cannot install the plugin, so
**subscribing on its own does nothing**.

### Step 1: Get the line models (choose one)

**A. Steam Workshop (recommended)**
1. Subscribe to [**Reverse Overlay - Guide Lines - ATS**](https://steamcommunity.com/sharedfiles/filedetails/?id=3822108979) on the Steam Workshop.
2. Wait until Steam has downloaded it.

**B. Without the Workshop**
1. Copy `optional\ats_reverse_overlay.scs` from the plugin download into
   `Documents\American Truck Simulator\mod\`.

Use **A or B, not both**.

### Step 2: Install the plugin

1. **Exit the game completely.**
2. Download `Reverse_Overlay_ATS_v<version>.zip`.
3. Before extracting, right-click the zip → **Properties**, tick **Unblock**
   if it's there, then click **OK**. This avoids extra Windows security prompts
   (see [section 7](#7-windows-security-unsigned-plugin)).
4. Extract the zip anywhere.
5. Double-click **`Install.bat`**.
   - It finds your ATS installation through Steam.
   - It copies `ats_reverse_overlay.dll` into
     `...\steamapps\common\American Truck Simulator\bin\win_x64\plugins\`
     and creates the `plugins` folder if it doesn't exist.
   - When it says **Installed …**, you can close the window.

To install by hand instead, copy `ats_reverse_overlay.dll` into
`<ATS folder>\bin\win_x64\plugins\`. Create the `plugins` folder if it's
missing. The game only loads plugins from that exact folder.

### Step 3: Enable the mod

1. Start ATS.
2. The game shows a notice that a plugin uses **advanced SDK features**. This is
   expected for any telemetry plugin. Confirm it.
3. Open the **Mod Manager**, enable **Reverse Overlay - Guide Lines - ATS**, and
   confirm the changes.
4. Load your profile, drive, and shift into **reverse**.

### Check it's working

Open `Documents\American Truck Simulator\ats_reverse_overlay.log`. After
starting the game you should see:

```
Reverse Overlay 0.1.0 starting in American Truck Simulator ...
engine functions located: ...
In-world guide lines ready (vehicle render hook installed).
```

After you first reverse:

```
In-world guide lines rendering: 134 pieces.
```

(The number varies.) If these lines are missing, see
[Troubleshooting](#6-troubleshooting).

---

## 3. Using it

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

File: `Documents\American Truck Simulator\ats_reverse_overlay.ini`.
It's created on the first game start. Edit it with any text editor. Most
changes apply **while you drive** (within about 2 seconds).

### `[world]` guide lines

| Setting | Default | Meaning |
|---|---|---|
| `enabled` | `1` | `0` turns the guide lines off |
| `red_until_m` | `3` | end of the red band, in metres of travel |
| `yellow_until_m` | `8` | end of the yellow band; green runs from here to the end |
| `line_height_m` | `0.35` | height of the lines; lower it if they float, raise it if they sink into the road |
| `allow_unverified` | `1` | `1`: also run on game versions not yet confirmed. `0`: only on confirmed versions (see [section 5](#5-game-updates)) |

### `[prediction]`

| Setting | Default | Meaning |
|---|---|---|
| `horizon_m` | `15` | how far ahead (backwards) the path is predicted, 3–60 m |

### `[overlay]` optional top-down panel

An optional bird's-eye panel showing the truck, the trailer and the predicted
area. **It's off by default.** It needs the DirectX 11 renderer, and turning it
on or off takes effect the **next time the game starts**.

| Setting | Default | Meaning |
|---|---|---|
| `enabled` | `0` | `1` shows the panel while reversing |
| `anchor` | `right` | `left`, `right`, `top_left`, `top_right`, `bottom_left`, `bottom_right` |
| `panel_height`, `aspect` | `0.42`, `0.75` | size relative to the screen height |
| `margin`, `offset_x`, `offset_y` | `0.02`, `0`, `0` | position fine-tuning |
| `opacity` | `0.9` | 0.2–1.0 |
| `always_show` | `0` | `1` shows it outside reverse (for positioning) |

`[hotkey]`: **Ctrl+Shift+F9** shows and hides the panel. It doesn't affect the
guide lines.

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

**Rolling back the game:** in Steam, right-click American Truck Simulator →
**Properties → Betas** and pick the previous version, if SCS offers it.

---

## 6. Troubleshooting

**Start with the log:** `Documents\American Truck Simulator\ats_reverse_overlay.log`.
The game's own log is `Documents\American Truck Simulator\game.log.txt`; search
it for `reverse-overlay` or `ats_reverse_overlay`.

| Symptom | Likely cause | Fix |
|---|---|---|
| No `ats_reverse_overlay.log` at all, or it isn't updated | The plugin isn't loaded | Check that the DLL is in `bin\win_x64\plugins\` (not just `bin\win_x64\`). In `game.log.txt`, look for `loading 'ats_reverse_overlay'`. If the game log shows the plugin failing to load, see [section 7](#7-windows-security-unsigned-plugin) |
| Log: *"Could not create /model/rto_guides/… - is the … mod enabled in the Mod Manager?"* | The line models aren't loaded | Enable **Reverse Overlay - Guide Lines - ATS** in the Mod Manager. Then restart the game, because after this error the lines stay off for the session |
| Log: *"engine functions missing"* | Unsupported game version | See [Game updates](#5-game-updates) |
| Log: *"In-world guide lines skipped: build not verified and allow_unverified = 0"* | Safety mode is on | Set `allow_unverified = 1` if you want to try it on this version |
| Log: *"In-world rendering raised an exception…"* | Something went wrong while drawing. The lines are switched off for the session | Restart the game. If it repeats, send the log with your report |
| Lines float above or sink into the road | Height estimate | Adjust `line_height_m` |
| Lines look too narrow or wide for your trailer | Body size is estimated from the wheels | Known limitation, see [Accuracy notes](#accuracy-notes) |
| Two sets of lines (blue/orange and red/yellow/green) | "Reverse Posture Assistant" is also installed | Use one or the other. Remove `ATSReverseTrajectoryRuntime.dll` from `plugins` and disable that mod |
| Ctrl+Shift+F9 does nothing | The hotkey only controls the optional panel | Expected. The lines are controlled with `[world] enabled` |

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
- It runs only inside ATS, loaded by the game's official plugin system.
- It reads telemetry, and it only writes `ats_reverse_overlay.log` and
  `ats_reverse_overlay.ini` in your `Documents\American Truck Simulator` folder.
- It has **no network access** and doesn't touch your save games.

---

## 8. Uninstall

1. Exit the game.
2. Run **`Uninstall.bat`** from the download, or delete
   `<ATS folder>\bin\win_x64\plugins\ats_reverse_overlay.dll`.
3. Disable or unsubscribe **Reverse Overlay - Guide Lines - ATS** (or delete
   `Documents\American Truck Simulator\mod\ats_reverse_overlay.scs`).
4. Optionally delete `ats_reverse_overlay.ini` and `ats_reverse_overlay.log`
   from `Documents\American Truck Simulator`.

---

## 9. FAQ

**Does it work in Convoy (multiplayer)?** Untested.

**Euro Truck Simulator 2?** A separate ETS2 version is planned.

**Mac or Linux?** Not currently.

**Does it affect performance?** Barely. It's about 140 small line pieces while
reversing and nothing otherwise.

**Can I change the colours?** Not yet. The band distances can be changed.

---

## 10. Credits and licences

- In-world drawing technique and base line model: *Reverse Posture Assistant
  For ATS* by yyysheng (MIT licence).
- SCS Telemetry SDK © SCS Software. MinHook © Tsuda Kageyu (BSD-2-Clause).
  Dear ImGui © Omar Cornut (MIT, used only by the optional panel).
- Licence texts are in the `LICENSES` folder of the download.
