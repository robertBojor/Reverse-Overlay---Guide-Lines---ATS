#pragma once

// Forward driving aid: two square dots ahead of the truck where its front
// corners will be after a fixed look-ahead time, red on the left and green on
// the right. With speed each dot stretches into a line along its corner's
// predicted path, centred on the same point. Truck only; trailers are
// ignored. Heights are added by the renderer.

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "guides.hpp"
#include "rig.hpp"

namespace rto
{
struct DotOptions
{
    double lookahead_s = 1.5;   // seconds of travel at the current speed
    double min_distance = 14.0; // metres, used when slow or stopped; clears a long hood from the cab
    double max_distance = 80.0; // metres
    double size = 0.5;          // dot edge length, metres
    double tile = 0.12;         // edge length of the dot model and width of a line piece
    double line_time = 0.5;     // line length in seconds of travel; 0 = always dots
    double max_line = 15.0;     // metres
    double line_spacing = 0.3;  // line piece length; the model is 0.36 m so pieces overlap
};

struct DotTile
{
    Vec2 centre;          // world
    double heading = 0.0; // the tile's edges line up with the predicted truck
    Band band = Band::dot_red;
};

inline double lookahead_distance(double speed, const DotOptions &o)
{
    const double farthest = std::max(o.max_distance, o.min_distance);
    return std::clamp(std::max(speed, 0.0) * o.lookahead_s, o.min_distance, farthest);
}

// Length of the forward lines. At or below the dot size the caller draws
// square dots instead.
inline double line_length(double speed, const DotOptions &o)
{
    return std::clamp(std::max(speed, 0.0) * o.line_time, 0.0, std::max(o.max_line, 0.0));
}

// Tractor pose after driving `distance` metres forward with constant
// curvature. The virtual rear axle follows an exact arc; this is the closed
// form of the bicycle model that predict_reverse() integrates.
inline Pose predict_forward(const UnitGeometry &tractor, const Pose &start, double distance)
{
    const double dh = tractor.curvature * distance;
    // Chord of the arc: distance * sin(dh/2) / (dh/2), towards the mid heading.
    const double half = dh * 0.5;
    const double chord = std::abs(half) < 1e-6 ? distance : distance * std::sin(half) / half;
    Pose end;
    end.heading = normalize_angle(start.heading + dh);
    const Vec2 axle = to_world(start, tractor.axle) + forward_dir(start.heading + half) * chord;
    end.p = axle - (right_dir(end.heading) * tractor.axle.x -
                    forward_dir(end.heading) * tractor.axle.z);
    return end;
}

// Exponential smoothing towards `target` with time constant `tau` seconds.
inline double smooth_toward(double current, double target, double dt, double tau)
{
    if (tau <= 0.0) return target;
    if (dt <= 0.0) return current;
    return current + (target - current) * (1.0 - std::exp(-dt / tau));
}

// Tiles for both dots, centred on the predicted front-left and front-right
// corners. A dot larger than one tile is an evenly overlapping n x n grid.
inline std::vector<DotTile> build_dots(const Rig &rig, double distance, const DotOptions &o = {})
{
    std::vector<DotTile> out;
    if (rig.units.empty() || rig.poses.empty()) return out;
    const UnitGeometry &tractor = rig.units[0];
    const Pose pose = predict_forward(tractor, rig.poses[0], distance);
    const auto corners = body_corners(tractor, pose);

    const double size = std::max(o.size, o.tile);
    const int n = static_cast<int>(std::ceil(size / o.tile - 1e-9));
    const double step = n > 1 ? (size - o.tile) / (n - 1) : 0.0;
    const double first = -(size - o.tile) * 0.5;
    const Vec2 right = right_dir(pose.heading);
    const Vec2 ahead = forward_dir(pose.heading);
    out.reserve(static_cast<std::size_t>(2 * n * n));
    for (const auto &[corner, band] : {std::pair{corners[0], Band::dot_red},
                                       std::pair{corners[1], Band::dot_green}})
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                out.push_back({corner + right * (first + i * step) + ahead * (first + j * step),
                               pose.heading, band});
    return out;
}

// Lines along the predicted front-left (red) and front-right (green) corner
// paths, from `centre - span/2` to `centre + span/2` metres of travel, as
// wide as a dot. Wider than one piece, a line is several parallel strips.
inline std::vector<GuideSegment> build_lines(const Rig &rig, double centre, double span,
                                             const DotOptions &o = {})
{
    std::vector<GuideSegment> out;
    if (rig.units.empty() || rig.poses.empty() || span <= 0.0) return out;
    const UnitGeometry &tractor = rig.units[0];
    const double nearest = std::max(centre - span * 0.5, 0.0);
    const double farthest = centre + span * 0.5;

    // Sample the corners finely, then cut each path into equal pieces.
    const int samples = std::max(2, static_cast<int>(std::ceil((farthest - nearest) / 0.1)) + 1);
    std::vector<detail::TracePoint> left, right;
    left.reserve(static_cast<std::size_t>(samples));
    right.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i)
    {
        const double d = nearest + (farthest - nearest) * i / (samples - 1);
        const auto c = body_corners(tractor, predict_forward(tractor, rig.poses[0], d));
        left.push_back({c[0], d});
        right.push_back({c[1], d});
    }

    const double size = std::max(o.size, o.tile);
    const int n = static_cast<int>(std::ceil(size / o.tile - 1e-9));
    const double step = n > 1 ? (size - o.tile) / (n - 1) : 0.0;
    const double first = -(size - o.tile) * 0.5;
    for (const auto &[trace, band] : {std::pair{&left, Band::red}, std::pair{&right, Band::green}})
    {
        const auto pts = detail::resample(*trace, o.line_spacing);
        for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const Vec2 a = pts[i].p, b = pts[i + 1].p;
            const double len = length(b - a);
            if (len < 1e-9) continue;
            const Vec2 ahead = (b - a) * (1.0 / len);
            const Vec2 side{-ahead.z, ahead.x}; // to the right of travel
            for (int k = 0; k < n; ++k)
            {
                const Vec2 shift = side * (first + k * step);
                out.push_back({a + shift, b + shift, band});
            }
        }
    }
    return out;
}
}
