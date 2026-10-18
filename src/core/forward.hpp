#pragma once

// Forward driving aid: one line on the road from the centre of the truck's
// front axle along the path it will take at the current steering. It grows
// with speed. Truck only; trailers are ignored. Heights are added by the
// renderer.

#include <algorithm>
#include <cmath>
#include <vector>

#include "guides.hpp"
#include "rig.hpp"

namespace rto
{
struct ForwardOptions
{
    double min_length = 25.0;  // metres, used when slow or stopped
    double length_time = 1.2;  // seconds of travel at the current speed
    double max_length = 40.0;  // metres
    double width = 0.3;        // metres, in whole strips of piece_width
    double piece_width = 0.15; // width of the line model
    double spacing = 1.0;      // length of the line model
    // The line is see-through, so pieces and strips meet edge to edge:
    // overlaps would show as darker bands.
    double max_turn = kPi / 2; // the line stops once the truck has turned this far
    // Curvature source by speed (m/s): wheel angles up to measured_from, the
    // truck's measured turn rate from measured_full, blended in between.
    double measured_from = 5.0;
    double measured_full = 10.0;
};

// Path curvature for the line. At speed the tyres slip, so the truck turns
// less than its wheel angles say and a long line would over-bend; the
// measured turn rate (rad/s) over the speed is the curvature it really
// drives. Slow, that ratio is noisy and the wheel angles are exact.
inline double forward_curvature(double wheel_curvature, double yaw_rate, double speed,
                                const ForwardOptions &o)
{
    if (speed <= o.measured_from) return wheel_curvature;
    const double measured = yaw_rate / speed;
    const double span = std::max(o.measured_full - o.measured_from, 1e-6);
    const double w = std::clamp((speed - o.measured_from) / span, 0.0, 1.0);
    return wheel_curvature + (measured - wheel_curvature) * w;
}

// Length of the line from the front axle: speed x time, within the limits.
inline double forward_length(double speed, const ForwardOptions &o)
{
    const double longest = std::max(o.max_length, o.min_length);
    return std::clamp(std::max(speed, 0.0) * o.length_time, o.min_length, longest);
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

namespace detail
{
// Truck travel for the forward lines: less on a tight turn, so they stop
// after a quarter turn instead of curling round.
inline double capped_span(const UnitGeometry &tractor, double span, const ForwardOptions &o)
{
    if (std::abs(tractor.curvature) > 1e-9)
        span = std::min(span, o.max_turn / std::abs(tractor.curvature));
    return span;
}

// Cuts a traced path into whole pieces and lays them as side-by-side strips
// to the line's width.
inline void lay_strips(const std::vector<TracePoint> &trace, Band band, const ForwardOptions &o,
                       std::vector<GuideSegment> &out)
{
    const int n = std::max(1, static_cast<int>(std::lround(o.width / o.piece_width)));
    const double first = -(n - 1) * o.piece_width * 0.5;
    const auto pts = resample(trace, o.spacing);
    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const Vec2 a = pts[i].p, b = pts[i + 1].p;
        const double len = length(b - a);
        // A short tail piece would still draw a whole model over the last one.
        if (len < o.spacing * 0.9) continue;
        const Vec2 ahead = (b - a) * (1.0 / len);
        const Vec2 side{-ahead.z, ahead.x}; // to the right of travel
        for (int k = 0; k < n; ++k)
        {
            const Vec2 shift = side * (first + k * o.piece_width);
            out.push_back({a + shift, b + shift, band});
        }
    }
}

// Shortest distance from `p` to the polyline, counting only points that
// project inside one of its segments; negative when none does.
inline double distance_alongside(const std::vector<Vec2> &line, Vec2 p)
{
    double best = -1.0;
    for (std::size_t i = 0; i + 1 < line.size(); ++i)
    {
        const Vec2 ab = line[i + 1] - line[i];
        const double len2 = dot(ab, ab);
        if (len2 < 1e-12) continue;
        const double t = dot(p - line[i], ab) / len2;
        if (t < 0.0 || t > 1.0) continue;
        const double d = length(p - (line[i] + ab * t));
        if (best < 0.0 || d < best) best = d;
    }
    return best;
}
}

// The path of the front axle's centre while the truck drives `span` metres
// (less on a tight turn, so it never curls round), cut into whole pieces and
// laid as side-by-side strips to the line's width.
inline std::vector<GuideSegment> build_forward(const Rig &rig, double span,
                                               const ForwardOptions &o = {})
{
    std::vector<GuideSegment> out;
    if (rig.units.empty() || rig.poses.empty() || span <= 0.0) return out;
    const UnitGeometry &tractor = rig.units[0];
    span = detail::capped_span(tractor, span, o);

    // Sample finely, then cut into equal pieces.
    const int samples = std::max(2, static_cast<int>(std::ceil(span / 0.2)) + 1);
    std::vector<detail::TracePoint> trace;
    trace.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i)
    {
        const double d = span * i / (samples - 1);
        trace.push_back({to_world(predict_forward(tractor, rig.poses[0], d), tractor.front_axle), d});
    }
    detail::lay_strips(trace, Band::blue, o, out);
    return out;
}

struct TrailerLine
{
    std::vector<GuideSegment> segments;
    // How far the last trailer's axle runs inside the front axle's path,
    // at most, over that stretch of road (metres).
    double offtrack = 0.0;
};

// Where the last trailer's inner wheels (on the inside of the turn) will run
// over the stretch of road the forward line covers. The rig is driven on
// until the trailer has crossed that stretch, and only the part alongside
// the forward line is kept, so it sits ahead of the truck next to it. On a
// turn a trailer cuts inside the truck: around a kingpin circling at radius
// R, its axle settles on radius sqrt(R^2 - d^2), d = kingpin to axle.
inline TrailerLine build_trailer_forward(const Rig &rig, double span,
                                         const ForwardOptions &o = {})
{
    TrailerLine out;
    if (rig.units.size() < 2 || rig.poses.size() != rig.units.size() || span <= 0.0) return out;
    const UnitGeometry &tractor = rig.units[0];
    span = detail::capped_span(tractor, span, o);

    const std::size_t last = rig.units.size() - 1;
    const UnitGeometry &g = rig.units[last];
    // Straight-line distance the trailer axle trails the front axle by: the
    // extra travel it needs to reach the far end of the forward line.
    const double reach = length(to_world(rig.poses[0], tractor.front_axle) -
                                to_world(rig.poses[last], g.axle));
    const Prediction p = predict_motion(rig, span + reach + 1.0, 1.0, 0.05, 0.2);
    if (p.steps.empty()) return out;

    std::vector<Vec2> front;
    for (const auto &step : p.steps)
        if (step.distance <= span + 1e-9) front.push_back(to_world(step.poses[0], tractor.front_axle));

    const double side = tractor.curvature >= 0.0 ? -1.0 : 1.0; // left turn: left wheels
    const Vec2 wheels{g.axle.x + side * g.half_width, g.axle.z};
    std::vector<detail::TracePoint> trace;
    for (const auto &step : p.steps)
    {
        const double d = detail::distance_alongside(front, to_world(step.poses[last], g.axle));
        if (d < 0.0)
        {
            if (!trace.empty()) break; // past the far end
            continue;                  // not yet level with the front axle's start
        }
        out.offtrack = std::max(out.offtrack, d);
        trace.push_back({to_world(step.poses[last], wheels), step.distance});
    }
    detail::lay_strips(trace, Band::teal, o, out.segments);
    return out;
}
}
