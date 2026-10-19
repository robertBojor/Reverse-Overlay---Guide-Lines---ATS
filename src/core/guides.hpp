#pragma once

// Backup-camera style guide lines in world space: the paths of the rearmost
// unit's two rear corners, split into fixed-length pieces and coloured by how
// far the rig has to travel to reach them. Heights are added by the renderer.

#include <cstddef>
#include <vector>

#include "rig.hpp"

namespace rto
{
enum class Band
{
    red,
    yellow,
    green,
    blue, // forward line
};

struct GuideSegment
{
    Vec2 from; // world
    Vec2 to;   // world
    Band band = Band::green;
};

struct GuideOptions
{
    double red_until = 3.0;     // metres of travel
    double yellow_until = 8.0;
    double spacing = 0.25;      // piece length; the model is 0.36 m so pieces overlap
    double clearance = 0.18;    // lines sit this far outside the body
    double tick_length = 0.6;   // inward ticks at band changes and at the end
};

inline Band band_for(double distance, const GuideOptions &o)
{
    if (distance < o.red_until) return Band::red;
    if (distance < o.yellow_until) return Band::yellow;
    return Band::green;
}

namespace detail
{
struct TracePoint
{
    Vec2 p;
    double distance; // rig travel at this sample
};

// Resample a polyline at equal arc-length spacing, carrying the rig distance.
inline std::vector<TracePoint> resample(const std::vector<TracePoint> &in, double spacing)
{
    std::vector<TracePoint> out;
    if (in.empty()) return out;
    out.push_back(in.front());
    double carry = 0.0;
    for (std::size_t i = 0; i + 1 < in.size(); ++i)
    {
        const Vec2 a = in[i].p, b = in[i + 1].p;
        const double len = length(b - a);
        if (len < 1e-9) continue;
        double t = spacing - carry;
        while (t <= len + 1e-9)
        {
            const double f = t / len;
            out.push_back({a + (b - a) * f,
                           in[i].distance + (in[i + 1].distance - in[i].distance) * f});
            t += spacing;
        }
        carry = len - (t - spacing);
    }
    if (length(in.back().p - out.back().p) > spacing * 0.3) out.push_back(in.back());
    return out;
}
}

inline std::vector<GuideSegment> build_guides(const Rig &rig, const Prediction &prediction,
                                              const GuideOptions &o = {})
{
    std::vector<GuideSegment> out;
    if (rig.units.empty() || prediction.steps.empty()) return out;
    const std::size_t last = rig.units.size() - 1;
    UnitGeometry g = rig.units[last];
    g.half_width += o.clearance;

    std::vector<detail::TracePoint> left, right;
    for (const auto &step : prediction.steps)
    {
        const auto c = body_corners(g, step.poses[last]);
        left.push_back({c[3], step.distance});
        right.push_back({c[2], step.distance});
    }
    for (const auto *trace : {&left, &right})
    {
        const auto pts = detail::resample(*trace, o.spacing);
        for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const double mid = (pts[i].distance + pts[i + 1].distance) * 0.5;
            out.push_back({pts[i].p, pts[i + 1].p, band_for(mid, o)});
        }
    }

    // Inward ticks across each line where the colour changes and at the end.
    const double end = prediction.steps.back().distance;
    std::vector<double> marks;
    if (o.red_until < end) marks.push_back(o.red_until);
    if (o.yellow_until < end) marks.push_back(o.yellow_until);
    marks.push_back(end);
    for (double mark : marks)
    {
        // Pose at the first sample at or beyond the mark.
        const PredictedStep *step = &prediction.steps.back();
        for (const auto &s : prediction.steps)
            if (s.distance >= mark - 1e-6)
            {
                step = &s;
                break;
            }
        const auto c = body_corners(g, step->poses[last]);
        const Vec2 across = c[2] - c[3]; // left -> right
        const double w = length(across);
        if (w < 1e-6) continue;
        const Vec2 dir = across * (1.0 / w);
        // Ticks take the colour of the band they close.
        const Band band = band_for(mark - 1e-3, o);
        const double tick = std::min(o.tick_length, w * 0.5);
        for (double t = 0.0; t + 1e-9 < tick; t += o.spacing)
        {
            const double t2 = std::min(t + o.spacing, tick);
            out.push_back({c[3] + dir * t, c[3] + dir * t2, band});
            out.push_back({c[2] - dir * t, c[2] - dir * t2, band});
        }
    }
    return out;
}
}
