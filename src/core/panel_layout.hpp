#pragma once

// Maps scene metres to panel pixels. Shared by the DX11 renderer and the SVG
// preview so both show the same framing.

#include <algorithm>
#include <cmath>
#include <vector>

#include "scene.hpp"

namespace rto
{
struct Point2f
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect
{
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

struct PanelView
{
    float pixels_per_metre = 10.0f;
    Vec2 centre;          // scene metres shown at the centre of the drawing area
    Point2f screen_centre;

    Point2f to_screen(Vec2 m) const
    {
        return {screen_centre.x + static_cast<float>((m.x - centre.x) * pixels_per_metre),
                screen_centre.y + static_cast<float>((m.z - centre.z) * pixels_per_metre)};
    }
};

// `ui_scale` is 1.0 at 1080p. The top band holds the status text.
inline Rect drawing_area(const Rect &panel, float ui_scale)
{
    const float margin = 14.0f * ui_scale;
    const float header = 72.0f * ui_scale;
    return {panel.x + margin, panel.y + header, panel.w - 2.0f * margin,
            panel.h - header - margin};
}

inline PanelView fit_panel(const Scene &scene, const Rect &panel, float ui_scale)
{
    const Rect area = drawing_area(panel, ui_scale);
    PanelView view;
    view.screen_centre = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
    const double w = std::max(scene.max.x - scene.min.x, 8.0);
    const double h = std::max(scene.max.z - scene.min.z, 12.0);
    view.pixels_per_metre = static_cast<float>(std::min(area.w / w, area.h / h));
    view.centre = {(scene.min.x + scene.max.x) * 0.5, (scene.min.z + scene.max.z) * 0.5};
    return view;
}

// Exponential smoothing so the framing does not jump as steering changes.
inline PanelView smooth_view(const PanelView &previous, const PanelView &target, float alpha)
{
    PanelView v = target;
    const auto lerp = [alpha](double a, double b) { return a + (b - a) * alpha; };
    v.pixels_per_metre = static_cast<float>(lerp(previous.pixels_per_metre, target.pixels_per_metre));
    v.centre = {lerp(previous.centre.x, target.centre.x), lerp(previous.centre.z, target.centre.z)};
    return v;
}

struct GridLine
{
    Point2f a, b;
};

// 5 m grid aligned to the tractor, clipped to the drawing area.
inline std::vector<GridLine> grid_lines(const PanelView &view, const Rect &panel,
                                        float ui_scale = 1.0f, double spacing = 5.0)
{
    std::vector<GridLine> lines;
    const Rect area = drawing_area(panel, ui_scale);
    if (view.pixels_per_metre <= 0.0f) return lines;
    const double half_w = area.w * 0.5 / view.pixels_per_metre;
    const double half_h = area.h * 0.5 / view.pixels_per_metre;
    const double x0 = std::ceil((view.centre.x - half_w) / spacing) * spacing;
    const double z0 = std::ceil((view.centre.z - half_h) / spacing) * spacing;
    for (double x = x0; x <= view.centre.x + half_w; x += spacing)
    {
        const auto p = view.to_screen({x, 0.0});
        lines.push_back({{p.x, area.y}, {p.x, area.y + area.h}});
    }
    for (double z = z0; z <= view.centre.z + half_h; z += spacing)
    {
        const auto p = view.to_screen({0.0, z});
        lines.push_back({{area.x, p.y}, {area.x + area.w, p.y}});
    }
    return lines;
}
}
