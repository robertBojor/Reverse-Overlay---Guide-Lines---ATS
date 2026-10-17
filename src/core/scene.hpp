#pragma once

// Turns a rig + prediction into renderer-agnostic 2D primitives expressed in
// the tractor's current local frame (metres, +x right, +z rear). Renderers map
// +z to "down" so the truck points up and the reverse path extends downwards.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "rig.hpp"

namespace rto
{
struct Color
{
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
};

constexpr Color with_alpha(Color c, std::uint8_t a) { return {c.r, c.g, c.b, a}; }

namespace palette
{
constexpr Color tractor{66, 165, 245, 255};
constexpr Color trailer{255, 152, 0, 255};
constexpr Color danger{244, 67, 54, 255};
constexpr Color wheel{230, 230, 230, 255};
constexpr Color grid{255, 255, 255, 28};
constexpr Color text{240, 240, 240, 255};
}

struct Shape
{
    enum class Kind
    {
        polyline,
        convex_fill
    };
    Kind kind = Kind::polyline;
    std::vector<Vec2> points;
    Color color;
    float thickness = 2.0f; // pixels at 1080p; renderer scales with resolution
    bool closed = false;
};

struct Label
{
    Vec2 at;
    std::string text;
    Color color = palette::text;
};

struct Scene
{
    bool visible = false;
    std::vector<Shape> shapes;
    std::vector<Label> labels;
    std::vector<std::string> status; // panel text lines
    bool warning = false;
    Vec2 min{0.0, 0.0};
    Vec2 max{0.0, 0.0};
};

namespace detail
{
inline void grow(Scene &scene, Vec2 p, bool &first)
{
    if (first)
    {
        scene.min = scene.max = p;
        first = false;
        return;
    }
    scene.min.x = std::min(scene.min.x, p.x);
    scene.min.z = std::min(scene.min.z, p.z);
    scene.max.x = std::max(scene.max.x, p.x);
    scene.max.z = std::max(scene.max.z, p.z);
}

inline Color unit_color(const UnitGeometry &g)
{
    return g.kind == UnitKind::tractor ? palette::tractor : palette::trailer;
}

inline std::string format(const char *fmt, double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), fmt, value);
    return buffer;
}
}

struct SceneOptions
{
    double tick_spacing = 5.0; // distance markers along the path
};

inline Scene build_scene(const Rig &rig, const Prediction &prediction,
                         const SceneOptions &options = {})
{
    Scene scene;
    if (rig.units.empty() || prediction.steps.empty()) return scene;
    scene.visible = true;

    const Pose origin = rig.poses[0];
    const auto local = [&](Vec2 world) { return to_local(origin, world); };
    const std::size_t last = rig.units.size() - 1;
    const bool jackknife = prediction.jackknife_distance >= 0.0;
    scene.warning = jackknife;

    // Corner traces per unit, in the tractor frame.
    // corners: 0 front-left, 1 front-right, 2 rear-right, 3 rear-left.
    std::vector<std::array<std::vector<Vec2>, 4>> traces(rig.units.size());
    for (const auto &step : prediction.steps)
        for (std::size_t u = 0; u < rig.units.size(); ++u)
        {
            const auto corners = body_corners(rig.units[u], step.poses[u]);
            for (int c = 0; c < 4; ++c) traces[u][c].push_back(local(corners[c]));
        }

    // Leading-edge sweep of the rearmost unit, filled between its rear corners.
    {
        const Color base = jackknife ? palette::danger : detail::unit_color(rig.units[last]);
        const auto &left = traces[last][3];
        const auto &right = traces[last][2];
        for (std::size_t i = 0; i + 1 < left.size(); ++i)
        {
            Shape quad;
            quad.kind = Shape::Kind::convex_fill;
            quad.color = with_alpha(base, 60);
            quad.points = {left[i], right[i], right[i + 1], left[i + 1]};
            scene.shapes.push_back(std::move(quad));
        }
        for (const auto *edge : {&left, &right})
        {
            Shape line;
            line.points = *edge;
            line.color = with_alpha(base, 235);
            line.thickness = 3.0f;
            scene.shapes.push_back(std::move(line));
        }
    }

    // Nose swing: the tractor's front corners swing wide when reversing with
    // steering applied. Trailer front corners stay inside the sweep.
    for (int c : {0, 1})
    {
        Shape line;
        line.points = traces[0][c];
        line.color = with_alpha(palette::tractor, 150);
        line.thickness = 1.5f;
        scene.shapes.push_back(std::move(line));
    }

    // Distance ticks across the leading edge.
    for (const auto &step : prediction.steps)
    {
        const double d = step.distance;
        if (d < options.tick_spacing - 1e-6) continue;
        const double r = std::fmod(d + 1e-6, options.tick_spacing);
        if (r > 0.2) continue;
        const auto corners = body_corners(rig.units[last], step.poses[last]);
        Shape tick;
        tick.points = {local(corners[3]), local(corners[2])};
        tick.color = with_alpha(palette::text, 170);
        tick.thickness = 1.5f;
        scene.shapes.push_back(tick);
        scene.labels.push_back({local(corners[2]) + Vec2{0.4, 0.0},
                                detail::format("%.0f m", d),
                                with_alpha(palette::text, 200)});
    }

    // Ghost outline of the rearmost unit at the end of the horizon.
    {
        const auto &end = prediction.steps.back();
        Shape ghost;
        ghost.closed = true;
        for (const auto &c : body_corners(rig.units[last], end.poses[last]))
            ghost.points.push_back(local(c));
        ghost.color = with_alpha(jackknife ? palette::danger : detail::unit_color(rig.units[last]), 120);
        ghost.thickness = 1.5f;
        scene.shapes.push_back(std::move(ghost));
    }

    // Current vehicle: solid outline plus wheels.
    for (std::size_t u = 0; u < rig.units.size(); ++u)
    {
        const UnitGeometry &g = rig.units[u];
        const Pose &pose = rig.poses[u];
        Shape body;
        body.closed = true;
        for (const auto &c : body_corners(g, pose)) body.points.push_back(local(c));
        Shape fill = body;
        fill.kind = Shape::Kind::convex_fill;
        fill.color = with_alpha(detail::unit_color(g), 45);
        scene.shapes.push_back(std::move(fill));
        body.color = detail::unit_color(g);
        body.thickness = 2.5f;
        scene.shapes.push_back(std::move(body));

        for (const auto &w : g.wheels)
        {
            const double hl = 0.5, hw = 0.16;
            const double s = std::sin(w.steering), c = std::cos(w.steering);
            Shape wheel;
            wheel.kind = Shape::Kind::convex_fill;
            wheel.color = with_alpha(palette::wheel, w.grounded ? 220 : 70);
            // Rotate the tyre rectangle by its steering angle (CCW = left).
            for (Vec2 corner : {Vec2{-hw, -hl}, Vec2{hw, -hl}, Vec2{hw, hl}, Vec2{-hw, hl}})
            {
                const Vec2 rotated{corner.x * c + corner.z * s, -corner.x * s + corner.z * c};
                wheel.points.push_back(local(to_world(pose, w.pos + rotated)));
            }
            scene.shapes.push_back(std::move(wheel));
        }
    }

    // Status text.
    for (std::size_t u = 1; u < rig.units.size(); ++u)
    {
        const double a = articulation(rig.poses[u - 1], rig.poses[u]) * 180.0 / kPi;
        std::string line = rig.units.size() > 2
            ? "Trailer " + std::to_string(u) + " angle "
            : "Trailer angle ";
        line += detail::format("%.0f\xC2\xB0", std::abs(a));
        // Positive articulation (trailer heading CCW of the tractor) puts the
        // trailer's tail to the right of the tractor's line.
        if (std::abs(a) >= 0.5) line += a > 0 ? ", tail right" : ", tail left";
        scene.status.push_back(line);
    }
    if (jackknife)
        scene.status.push_back(
            detail::format("JACKKNIFE in %.1f m", prediction.jackknife_distance));

    bool first = true;
    for (const auto &shape : scene.shapes)
        for (const auto &p : shape.points) detail::grow(scene, p, first);
    return scene;
}
}
