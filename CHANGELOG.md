# Changelog

## 0.3.0

**New**
- **The forward dots stretch into lines with speed.** Each dot becomes a line
  along the path its corner will take, centred on the same point and curving
  with the steering: half a second of travel long, at most 15 m. New
  `[forward]` settings `line_time_s` and `max_line_m`; `line_time_s = 0`
  keeps square dots.

**Changed**
- **Forward dots are visible from the cab.** They now sit at least 14 m
  ahead (was 6 m), so they clear a long hood such as the W900's, and are
  0.5 m squares (was 0.12 m), so they still read as dots at the shallow
  viewing angle from the driver's seat.

**Upgrading**
- From 0.2.0: update the plugin only. The mod is unchanged, so the Workshop
  item stays at 0.2.0. From 0.1.0: update the mod as well.
- A settings file written by 0.2.0 keeps the old values. Set
  `min_distance_m = 14` and `dot_size_m = 0.5` under `[forward]`, or delete
  the file to get a fresh copy.

## 0.2.0

**Renamed** from *Reverse Overlay - Guide Lines - ATS* to
**Backing & Lane Prediction - ATS**. The Workshop item, the plugin file
(`ats_reverse_overlay.dll`) and the settings, log and model paths are
unchanged, so updating needs no clean-up.

**New**
- **Forward dots for lane position.** In any forward gear, two square dots
  appear on the road where the truck's front corners will be about 1.5 s from
  now: red for the front-left corner, green for the front-right. They are at
  least 6 m ahead when slow or stopped. Steering is smoothed so the dots stay
  steady at speed. Trailers are ignored.
- **Ctrl+Shift+F9 cycles what is shown:** forward dots only → reverse lines
  only → both → nothing. The choice is saved to
  `Documents\American Truck Simulator\ats_reverse_overlay.mode` and
  restored on the next start. The first start is on *both*.
- **`[forward]` settings:** `lookahead_s`, `min_distance_m`,
  `max_distance_m`, `dot_size_m`, `dot_height_m`, `steering_smoothing_s`.
  All of them apply while you drive.

**Removed**
- The optional top-down panel and its `[overlay]` settings. An `[overlay]`
  section left in an older settings file is ignored. The plugin no longer
  hooks DirectX or ships Dear ImGui.

**Upgrading**
- Update both the plugin and the mod: the plugin needs the new dot models
  (`/model/rto_guides/dot_red.pmd`, `dot_green.pmd`).
- Your settings file is kept. The new `[forward]` settings use their defaults
  until you add them. Delete the file to get a fresh commented copy.

## 0.1.0

- First release: in-world reverse guide lines for the truck and up to ten
  trailers, red / yellow / green by distance, visible in every camera and the
  mirrors. Optional top-down panel.
