#pragma once

// Representative ATS rigs for tests and previews. Dimensions are typical of a
// long-hood tandem tractor and a 53 ft tandem van; local frame +x right, +z rear.

#include "../src/core/rig.hpp"

namespace fixtures
{
using namespace rto;

inline UnitInput tractor(Pose placement, double steering_left = 0.0)
{
    UnitInput u;
    u.placement = placement;
    const double L = 5.2; // front axle to first drive axle
    // Ackermann front wheels for the requested centre-line angle.
    double left = steering_left, right = steering_left;
    if (std::abs(steering_left) > 1e-6)
    {
        const double rear_z = 0.65; // tandem centre
        const double wheelbase = L + rear_z;
        const double radius = wheelbase / std::tan(steering_left); // +left
        left = std::atan(wheelbase / (radius - 1.0));
        right = std::atan(wheelbase / (radius + 1.0));
    }
    u.wheels = {
        {{-1.0, -L}, true, false, true, left},
        {{1.0, -L}, true, false, true, right},
        {{-0.9, 0.0}, false, false, true, 0.0},
        {{0.9, 0.0}, false, false, true, 0.0},
        {{-0.9, 1.3}, false, false, true, 0.0},
        {{0.9, 1.3}, false, false, true, 0.0},
    };
    u.hook = Vec2{0.0, 0.45};
    return u;
}

inline UnitInput trailer_behind(const UnitInput &parent, Vec2 coupling_on_parent,
                                double articulation, double kingpin_to_axle = 12.0,
                                double body_scale = 1.0)
{
    UnitInput t;
    const Vec2 kingpin{0.0, -6.0};
    const double axle = kingpin.z + kingpin_to_axle;
    t.hook = kingpin;
    t.wheels = {
        {{-0.9, axle - 0.62 * body_scale}, false, false, true, 0.0},
        {{0.9, axle - 0.62 * body_scale}, false, false, true, 0.0},
        {{-0.9, axle + 0.62 * body_scale}, false, false, true, 0.0},
        {{0.9, axle + 0.62 * body_scale}, false, false, true, 0.0},
    };
    const Vec2 hitch_world = to_world(parent.placement, coupling_on_parent);
    t.placement.heading = normalize_angle(parent.placement.heading + articulation);
    t.placement.p = hitch_world - (right_dir(t.placement.heading) * kingpin.x -
                                   forward_dir(t.placement.heading) * kingpin.z);
    return t;
}

inline RigInput single(double steering_left, double articulation, double heading = 0.0)
{
    RigInput in;
    in.truck = tractor({{100.0, 200.0}, heading}, steering_left);
    in.trailers.push_back(trailer_behind(in.truck, *in.truck.hook, articulation));
    in.wheel_steering_valid = true;
    return in;
}

inline RigInput bobtail(double steering_left, double heading = 0.0)
{
    RigInput in;
    in.truck = tractor({{0.0, 0.0}, heading}, steering_left);
    in.wheel_steering_valid = true;
    return in;
}

// 28 ft doubles: lead trailer, converter dolly, second trailer.
inline RigInput doubles(double steering_left, double art1, double art2)
{
    RigInput in;
    in.truck = tractor({{0.0, 0.0}, 0.0}, steering_left);
    auto lead = trailer_behind(in.truck, *in.truck.hook, art1, 6.4, 0.0);
    in.trailers.push_back(lead);
    // Pintle hook 1.2 m behind the lead trailer's axle.
    const Vec2 pintle{0.0, lead.wheels[0].pos.z + 1.2};
    auto second = trailer_behind(lead, pintle, art2, 6.4 + 2.4, 0.0);
    in.trailers.push_back(second);
    in.wheel_steering_valid = true;
    return in;
}
}
