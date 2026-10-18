#pragma once

// Forward driving aid: two square dots ahead of the truck where its front
// corners will be after a fixed look-ahead time, red on the left and green on
// the right. Truck only; trailers are ignored. Heights are added by the
// renderer.

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
    double min_distance = 6.0;  // metres, used when slow or stopped
    double max_distance = 80.0; // metres
    double size = 0.12;         // dot edge length, metres
    double tile = 0.12;         // edge length of the dot model; bigger dots are tiled
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
}
