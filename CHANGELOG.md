# Changelog

## 0.2.0

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
