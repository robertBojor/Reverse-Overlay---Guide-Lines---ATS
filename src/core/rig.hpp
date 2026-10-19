#pragma once

// Vehicle geometry and low-speed reverse prediction for a tractor and a chain
// of trailers. Platform neutral: no Windows or SCS SDK types in here.
//
// Coordinate conventions follow the SCS telemetry SDK:
//   world:  +x east, +z south (top-down plane, y is ignored)
//   local:  +x right, +z rear, origin at the unit's telemetry placement
//   heading: radians, 0 = north (-z), counter-clockwise positive (left)
//   steering: radians, counter-clockwise positive (left)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace rto
{
constexpr double kPi = 3.14159265358979323846;

struct Vec2
{
    double x = 0.0;
    double z = 0.0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.z + b.z}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.z - b.z}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.z * s}; }
inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.z * b.z; }
inline double length(Vec2 a) { return std::hypot(a.x, a.z); }

inline double normalize_angle(double a)
{
    a = std::fmod(a + kPi, 2.0 * kPi);
    if (a < 0.0) a += 2.0 * kPi;
    return a - kPi;
}

struct Pose
{
    Vec2 p;
    double heading = 0.0;
};

inline Vec2 forward_dir(double heading) { return {-std::sin(heading), -std::cos(heading)}; }
inline Vec2 right_dir(double heading) { return {std::cos(heading), -std::sin(heading)}; }

inline Vec2 to_world(const Pose &pose, Vec2 local)
{
    return pose.p + right_dir(pose.heading) * local.x -
           forward_dir(pose.heading) * local.z;
}

inline Vec2 to_local(const Pose &pose, Vec2 world)
{
    const Vec2 d = world - pose.p;
    return {dot(d, right_dir(pose.heading)), -dot(d, forward_dir(pose.heading))};
}

// ---------------------------------------------------------------------------
// Inputs (filled from telemetry)

struct WheelInput
{
    Vec2 pos;                 // local
    bool steerable = false;
    bool liftable = false;
    bool grounded = true;     // false when lifted / off the ground
    double steering = 0.0;    // radians, CCW positive; 0 if unknown
};

struct UnitInput
{
    Pose placement;                 // world
    std::vector<WheelInput> wheels; // local
    std::optional<Vec2> hook;       // truck: fifth wheel; trailer: kingpin (local)
};

struct RigInput
{
    UnitInput truck;
    std::vector<UnitInput> trailers; // connected chain, nearest first
    // Fallback when per-wheel steering is unavailable: effective steering
    // input <-1, 1> (CCW positive) scaled by an assumed lock angle.
    double steering_input = 0.0;
    bool wheel_steering_valid = false;
};

// ---------------------------------------------------------------------------
// Derived geometry

enum class UnitKind
{
    tractor,
    trailer
};

struct UnitGeometry
{
    UnitKind kind = UnitKind::tractor;
    // Body outline in local coordinates (front_z < rear_z).
    double front_z = 0.0;
    double rear_z = 0.0;
    double half_width = 1.3;
    // Tractor: virtual rear axle. Trailer: equivalent axle.
    Vec2 axle;
    // Tractor: centre of the front (steering) axle group.
    Vec2 front_axle;
    // Trailer only: kingpin (local) and where it couples on the parent (parent local).
    Vec2 kingpin;
    Vec2 coupling_on_parent;
    // Tractor: path curvature (1/m, CCW positive while driving forward).
    double curvature = 0.0;
    double wheelbase = 0.0;
    // Trailer: equivalent axle steering angle (self-steering axles).
    double axle_steering = 0.0;
    std::vector<WheelInput> wheels; // for drawing
};

struct Rig
{
    std::vector<UnitGeometry> units; // [0] = tractor, then trailers
    std::vector<Pose> poses;         // current world placement of each unit
};

namespace tuning
{
// SCS telemetry exposes wheel and hitch positions but no body dimensions, so
// body extents are estimated from the running gear.
constexpr double tractor_front_overhang = 1.35;  // front axle -> bumper
constexpr double tractor_rear_overhang = 0.80;   // rearmost axle -> frame end
constexpr double trailer_kingpin_setback = 0.95; // kingpin -> trailer front
constexpr double trailer_min_tail = 0.60;        // rearmost axle -> rear, minimum
constexpr double trailer_tail_ratio = 0.22;      // axle-group tail vs kingpin distance
constexpr double tire_half_width = 0.25;
constexpr double max_half_width = 1.30;          // 102 in. legal width
constexpr double min_half_width = 0.95;
constexpr double fallback_lock_angle = 0.66;     // ~38 deg, used without wheel data
constexpr double jackknife_angle = 85.0 * kPi / 180.0;
}

inline double clamp_half_width(const std::vector<WheelInput> &wheels)
{
    if (wheels.empty()) return tuning::max_half_width;
    double min_x = wheels.front().pos.x, max_x = min_x;
    for (const auto &w : wheels)
    {
        min_x = std::min(min_x, w.pos.x);
        max_x = std::max(max_x, w.pos.x);
    }
    return std::clamp((max_x - min_x) * 0.5 + tuning::tire_half_width,
                      tuning::min_half_width, tuning::max_half_width);
}

inline UnitGeometry derive_tractor(const RigInput &input)
{
    UnitGeometry g;
    g.kind = UnitKind::tractor;
    g.wheels = input.truck.wheels;
    const auto &wheels = input.truck.wheels;
    if (wheels.size() < 2)
    {
        g.wheelbase = 5.0;
        g.axle = {0.0, 0.0};
        g.front_axle = {0.0, -g.wheelbase};
        g.front_z = -6.5;
        g.rear_z = 1.0;
        g.curvature = std::tan(std::clamp(input.steering_input, -1.0, 1.0) *
                               tuning::fallback_lock_angle) / g.wheelbase;
        return g;
    }

    double min_z = wheels.front().pos.z, max_z = min_z;
    for (const auto &w : wheels)
    {
        min_z = std::min(min_z, w.pos.z);
        max_z = std::max(max_z, w.pos.z);
    }

    // Front steering group: steerable wheels ahead of the first fixed axle.
    // A steerable tag/pusher axle behind it belongs to the rear support group.
    double first_fixed_z = max_z + 1.0;
    for (const auto &w : wheels)
        if (!w.steerable && w.grounded) first_fixed_z = std::min(first_fixed_z, w.pos.z);

    std::vector<const WheelInput *> front, rear;
    for (const auto &w : wheels)
    {
        if (w.steerable && w.pos.z < first_fixed_z - 0.2)
            front.push_back(&w);
        else if (w.grounded)
            rear.push_back(&w);
    }

    double front_z = min_z;
    if (!front.empty())
    {
        front_z = 0.0;
        for (const auto *w : front) front_z += w->pos.z;
        front_z /= static_cast<double>(front.size());
    }
    double rear_z = max_z;
    if (!rear.empty())
    {
        rear_z = 0.0;
        for (const auto *w : rear) rear_z += w->pos.z;
        rear_z /= static_cast<double>(rear.size());
    }

    g.axle = {0.0, rear_z};
    g.front_axle = {0.0, front_z};
    g.wheelbase = std::max(rear_z - front_z, 1.5);

    // Curvature from the actual wheel angles. For a wheel at lateral offset x
    // and distance L ahead of the rear axle, steering angle d gives
    // curvature k = tan d / (L - x tan d); left and right Ackermann angles
    // agree on k, so averaging them is exact for a correct linkage.
    double k_sum = 0.0;
    std::size_t k_count = 0;
    if (input.wheel_steering_valid)
    {
        for (const auto *w : front)
        {
            const double t = std::tan(std::clamp(w->steering, -1.2, 1.2));
            const double L = rear_z - w->pos.z;
            const double denom = L - w->pos.x * t;
            if (L > 0.5 && std::abs(denom) > 0.2)
            {
                k_sum += t / denom;
                ++k_count;
            }
        }
    }
    g.curvature = k_count
        ? k_sum / static_cast<double>(k_count)
        : std::tan(std::clamp(input.steering_input, -1.0, 1.0) *
                   tuning::fallback_lock_angle) / g.wheelbase;

    g.front_z = min_z - tuning::tractor_front_overhang;
    g.rear_z = max_z + tuning::tractor_rear_overhang;
    g.half_width = clamp_half_width(wheels);
    return g;
}

inline UnitGeometry derive_trailer(const UnitInput &unit, const UnitInput &parent_unit,
                                   const UnitGeometry &parent)
{
    UnitGeometry g;
    g.kind = UnitKind::trailer;
    g.wheels = unit.wheels;
    g.kingpin = unit.hook.value_or(Vec2{0.0, 0.0});

    double axle_z_sum = 0.0, tan_sum = 0.0, max_z = g.kingpin.z + 4.0;
    std::size_t active = 0;
    bool any = false;
    for (const auto &w : unit.wheels)
    {
        max_z = any ? std::max(max_z, w.pos.z) : w.pos.z;
        any = true;
        if (!w.grounded) continue;
        axle_z_sum += w.pos.z;
        tan_sum += std::tan(std::clamp(w.steering, -1.0, 1.0));
        ++active;
    }
    double axle_z;
    if (active)
    {
        axle_z = axle_z_sum / static_cast<double>(active);
        g.axle_steering = std::atan(tan_sum / static_cast<double>(active));
    }
    else
    {
        axle_z = g.kingpin.z + 10.0;
    }
    if (!any) max_z = axle_z;
    if (axle_z - g.kingpin.z < 1.0) axle_z = g.kingpin.z + 1.0;
    g.axle = {g.kingpin.x, axle_z};

    const double d = axle_z - g.kingpin.z;
    g.front_z = g.kingpin.z - tuning::trailer_kingpin_setback;
    g.rear_z = std::max(max_z + tuning::trailer_min_tail,
                        axle_z + std::clamp(d * tuning::trailer_tail_ratio, 1.0, 4.0));
    g.half_width = clamp_half_width(unit.wheels);
    g.half_width = std::max(g.half_width, 1.2); // trailer bodies overhang the wheels

    // Measure the coupling point live: where this trailer's kingpin sits in
    // the parent's frame. This needs no rear-hitch data for doubles/triples.
    const Vec2 kingpin_world = to_world(unit.placement, g.kingpin);
    g.coupling_on_parent = to_local(parent_unit.placement, kingpin_world);
    (void)parent;
    return g;
}

inline Rig build_rig(const RigInput &input)
{
    Rig rig;
    rig.units.push_back(derive_tractor(input));
    rig.poses.push_back(input.truck.placement);
    const UnitInput *parent_unit = &input.truck;
    for (const auto &trailer : input.trailers)
    {
        rig.units.push_back(derive_trailer(trailer, *parent_unit, rig.units.back()));
        rig.poses.push_back(trailer.placement);
        parent_unit = &trailer;
    }
    return rig;
}

// ---------------------------------------------------------------------------
// Prediction

struct PredictedStep
{
    double distance = 0.0;      // metres travelled in reverse
    std::vector<Pose> poses;    // per unit
};

struct Prediction
{
    std::vector<PredictedStep> steps;
    double jackknife_distance = -1.0; // first distance where a joint exceeds the limit
    std::size_t jackknife_unit = 0;
};

inline double articulation(const Pose &parent, const Pose &child)
{
    return normalize_angle(child.heading - parent.heading);
}

// Integrate the rig backwards by `horizon` metres with constant steering.
// Tractor: kinematic bicycle about its virtual rear axle.
// Trailers: no-side-slip constraint at the equivalent axle, driven by the
// actual displacement of the coupling point (handles off-axle hitches).
inline Prediction predict_reverse(const Rig &rig, double horizon = 15.0,
                                  double step = 0.05, double sample_every = 0.25)
{
    Prediction out;
    if (rig.units.empty()) return out;
    horizon = std::clamp(horizon, 1.0, 60.0);
    step = std::clamp(step, 0.01, 0.25);
    sample_every = std::max(sample_every, step);

    std::vector<Pose> poses = rig.poses;
    const UnitGeometry &tractor = rig.units[0];
    out.steps.push_back({0.0, poses});

    double next_sample = sample_every;
    const int steps = static_cast<int>(std::ceil(horizon / step));
    for (int i = 1; i <= steps; ++i)
    {
        const double travelled = std::min(i * step, horizon);
        const double ds = -(travelled - (i - 1) * step); // negative: reversing
        const std::vector<Pose> before = poses;

        // Tractor rear axle.
        Pose &t = poses[0];
        Vec2 axle = to_world(t, tractor.axle);
        const double dh = tractor.curvature * ds;
        const double mid = t.heading + dh * 0.5;
        axle = axle + forward_dir(mid) * ds;
        t.heading = normalize_angle(t.heading + dh);
        t.p = axle - (right_dir(t.heading) * tractor.axle.x -
                      forward_dir(t.heading) * tractor.axle.z);

        // Trailers, each driven by its coupling point's displacement.
        for (std::size_t u = 1; u < rig.units.size(); ++u)
        {
            const UnitGeometry &g = rig.units[u];
            const Vec2 hitch_old = to_world(before[u - 1], g.coupling_on_parent);
            const Vec2 hitch_new = to_world(poses[u - 1], g.coupling_on_parent);
            const Vec2 dh_vec = hitch_new - hitch_old;
            const double d = std::max(g.axle.z - g.kingpin.z, 0.5);
            const double phi = std::clamp(g.axle_steering, -1.0, 1.0);
            const double denom = d * std::max(std::cos(phi), 0.25);
            const double theta0 = before[u].heading;
            double theta = theta0;
            for (int it = 0; it < 4; ++it)
            {
                const double mid_theta = theta0 + normalize_angle(theta - theta0) * 0.5;
                const Vec2 n = right_dir(mid_theta + phi);
                theta = normalize_angle(theta0 - dot(dh_vec, n) / denom);
            }
            Pose &p = poses[u];
            p.heading = theta;
            p.p = hitch_new - (right_dir(theta) * g.kingpin.x -
                               forward_dir(theta) * g.kingpin.z);

            if (out.jackknife_distance < 0.0 &&
                std::abs(articulation(poses[u - 1], p)) > tuning::jackknife_angle)
            {
                out.jackknife_distance = travelled;
                out.jackknife_unit = u;
            }
        }

        if (travelled + 1e-9 >= next_sample || i == steps)
        {
            out.steps.push_back({travelled, poses});
            next_sample += sample_every;
        }
        if (out.jackknife_distance >= 0.0) break;
    }
    return out;
}


// Body corners in world space: front-left, front-right, rear-right, rear-left.
inline std::array<Vec2, 4> body_corners(const UnitGeometry &g, const Pose &pose)
{
    return {to_world(pose, {-g.half_width, g.front_z}),
            to_world(pose, {g.half_width, g.front_z}),
            to_world(pose, {g.half_width, g.rear_z}),
            to_world(pose, {-g.half_width, g.rear_z})};
}
}
