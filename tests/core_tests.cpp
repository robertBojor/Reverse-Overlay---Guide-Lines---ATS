#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>

#include "../src/core/guides.hpp"
#include "../src/core/scene.hpp"
#include "fixtures.hpp"

using namespace rto;

static int checks = 0;
#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        ++checks;                                                                \
        if (!(cond))                                                             \
        {                                                                        \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            return 1;                                                            \
        }                                                                        \
    } while (0)

static bool near(double a, double b, double eps) { return std::abs(a - b) <= eps; }
static double deg(double r) { return r * 180.0 / kPi; }

int main()
{
    // Frame conversions round-trip and match the SDK heading convention.
    {
        const Pose west{{10.0, 5.0}, kPi / 2.0}; // heading 0.25 rot = facing west
        const Vec2 ahead = to_world(west, {0.0, -1.0});
        CHECK(near(ahead.x, 9.0, 1e-9) && near(ahead.z, 5.0, 1e-9));
        const Vec2 right = to_world(west, {1.0, 0.0});
        CHECK(near(right.x, 10.0, 1e-9) && near(right.z, 4.0, 1e-9)); // right of west = north
        const Vec2 back = to_local(west, to_world(west, {0.3, -2.0}));
        CHECK(near(back.x, 0.3, 1e-9) && near(back.z, -2.0, 1e-9));
    }

    // Geometry: wheelbase from the front axle to the drive-tandem centre,
    // Ackermann curvature equals the centre-line bicycle value.
    {
        const double centre = 0.4;
        const Rig rig = build_rig(fixtures::bobtail(centre));
        const auto &g = rig.units[0];
        CHECK(near(g.wheelbase, 5.85, 1e-9));
        CHECK(near(g.axle.z, 0.65, 1e-9));
        CHECK(near(g.curvature, std::tan(centre) / 5.85, 1e-6));
    }

    // Lifted tag axle moves the virtual rear axle forward.
    {
        auto in = fixtures::bobtail(0.0);
        in.truck.wheels[4].grounded = in.truck.wheels[5].grounded = false;
        CHECK(near(build_rig(in).units[0].axle.z, 0.0, 1e-9));
    }

    // Fallback steering without per-wheel data.
    {
        auto in = fixtures::bobtail(0.0);
        in.wheel_steering_valid = false;
        in.steering_input = 1.0;
        CHECK(build_rig(in).units[0].curvature > 0.0);
    }

    // Straight reverse: tractor goes straight back, trailer stays aligned.
    {
        const Rig rig = build_rig(fixtures::single(0.0, 0.0));
        const Prediction p = predict_reverse(rig, 10.0);
        CHECK(p.jackknife_distance < 0.0);
        const auto &end = p.steps.back();
        CHECK(near(p.steps.back().distance, 10.0, 1e-9));
        CHECK(near(end.poses[0].p.x, rig.poses[0].p.x, 1e-6));
        CHECK(near(end.poses[0].p.z - rig.poses[0].p.z, 10.0, 1e-6)); // heading 0: rear = +z
        CHECK(near(end.poses[1].heading, 0.0, 1e-9));
    }

    // Live coupling measurement recovers the fifth-wheel position.
    {
        const auto in = fixtures::single(0.0, 0.5);
        const Rig rig = build_rig(in);
        CHECK(near(rig.units[1].coupling_on_parent.x, 0.0, 1e-9));
        CHECK(near(rig.units[1].coupling_on_parent.z, 0.45, 1e-9));
        CHECK(near(rig.units[1].axle.z - rig.units[1].kingpin.z, 12.0, 1e-9));
    }

    // Bobtail, wheels turned left, reversing: the rear swings left and the
    // heading turns clockwise (nose goes right).
    {
        const Rig rig = build_rig(fixtures::bobtail(0.5));
        const Prediction p = predict_reverse(rig, 5.0);
        const Pose &end = p.steps.back().poses[0];
        const Vec2 axle_local = to_local(rig.poses[0], to_world(end, rig.units[0].axle));
        CHECK(axle_local.x < -0.5);
        CHECK(end.heading < 0.0);
        // Follows a circle of radius 1/k about the instantaneous centre.
        const double R = 1.0 / rig.units[0].curvature;
        const Vec2 centre = to_world(rig.poses[0], {-R, rig.units[0].axle.z});
        CHECK(near(length(to_world(end, rig.units[0].axle) - centre), R, 1e-3));
    }

    // Classic trailer behaviour: steering left in reverse pushes the
    // trailer's tail to the right and increases articulation to the right
    // (trailer heading clockwise relative to the tractor).
    {
        const Rig rig = build_rig(fixtures::single(0.3, 0.0));
        const Prediction p = predict_reverse(rig, 4.0);
        const auto &end = p.steps.back();
        // Trailer yaws counter-clockwise: its tail swings right of the line
        // it would follow when reversing straight.
        CHECK(end.poses[1].heading > rig.poses[1].heading + 0.01);
        const auto tail = body_corners(rig.units[1], end.poses[1]);
        const Vec2 tail_mid = (tail[2] + tail[3]) * 0.5;
        CHECK(to_local(rig.poses[1], tail_mid).x > 0.0);
        CHECK(articulation(end.poses[0], end.poses[1]) > 0.0);
    }

    // Constant steering in reverse eventually jackknifes; the prediction
    // stops there and the scene reports it.
    {
        const Rig rig = build_rig(fixtures::single(0.35, 0.0));
        const Prediction p = predict_reverse(rig, 60.0);
        CHECK(p.jackknife_distance > 2.0);
        CHECK(p.jackknife_unit == 1);
        CHECK(std::abs(deg(articulation(p.steps.back().poses[0], p.steps.back().poses[1]))) > 84.0);
        const Scene s = build_scene(rig, p);
        CHECK(s.warning);
        CHECK(!s.status.empty() && s.status.back().rfind("JACKKNIFE", 0) == 0);
    }

    // Positive articulation puts the trailer's tail to the right. Reversing
    // straight makes it worse; steering right (tractor follows the tail)
    // straightens it.
    {
        const double art = 20.0 * kPi / 180.0;
        const Rig rig = build_rig(fixtures::single(0.0, art));
        const Prediction p = predict_reverse(rig, 3.0);
        // Straight tractor with articulated trailer: articulation grows in reverse.
        CHECK(std::abs(articulation(p.steps.back().poses[0], p.steps.back().poses[1])) > art);
        const Rig follow = build_rig(fixtures::single(-0.45, art));
        const Prediction q = predict_reverse(follow, 3.0);
        CHECK(std::abs(articulation(q.steps.back().poses[0], q.steps.back().poses[1])) < art);
    }

    // Kinematics are frame-independent: rotating the whole rig rotates the result.
    {
        const Rig a = build_rig(fixtures::single(0.25, 0.1, 0.0));
        const Rig b = build_rig(fixtures::single(0.25, 0.1, 1.1));
        const Prediction pa = predict_reverse(a, 8.0);
        const Prediction pb = predict_reverse(b, 8.0);
        const double art_a = articulation(pa.steps.back().poses[0], pa.steps.back().poses[1]);
        const double art_b = articulation(pb.steps.back().poses[0], pb.steps.back().poses[1]);
        CHECK(near(art_a, art_b, 1e-6));
    }

    // Doubles: three units, second trailer reacts more than the first.
    {
        const Rig rig = build_rig(fixtures::doubles(0.2, 0.0, 0.0));
        CHECK(rig.units.size() == 3);
        CHECK(near(rig.units[2].coupling_on_parent.z, rig.units[1].wheels[0].pos.z + 1.2, 1e-9));
        const Prediction p = predict_reverse(rig, 3.0);
        const auto &end = p.steps.back().poses;
        CHECK(std::abs(articulation(end[1], end[2])) > 0.0);
        const Scene s = build_scene(rig, p);
        CHECK(s.status.size() >= 2);
    }

    // Integration step size barely changes the answer.
    {
        const Rig rig = build_rig(fixtures::single(0.3, 0.05));
        const Prediction coarse = predict_reverse(rig, 10.0, 0.1);
        const Prediction fine = predict_reverse(rig, 10.0, 0.01);
        const double a = articulation(coarse.steps.back().poses[0], coarse.steps.back().poses[1]);
        const double b = articulation(fine.steps.back().poses[0], fine.steps.back().poses[1]);
        CHECK(near(a, b, 0.01));
    }

    // Guide lines: evenly spaced pieces, banded by travel distance, starting
    // at the rear corners (plus clearance) of the rearmost unit.
    {
        const Rig rig = build_rig(fixtures::single(0.0, 0.0));
        const Prediction p = predict_reverse(rig, 15.0);
        GuideOptions o;
        const auto guides = build_guides(rig, p, o);
        CHECK(!guides.empty());
        std::size_t red = 0, yellow = 0, green = 0;
        double max_piece = 0.0;
        for (const auto &s : guides)
        {
            max_piece = std::max(max_piece, length(s.to - s.from));
            red += s.band == Band::red;
            yellow += s.band == Band::yellow;
            green += s.band == Band::green;
        }
        CHECK(max_piece <= o.spacing + 1e-6);
        // Straight reverse: band lengths 3, 5, 7 m on two lines (+ ticks).
        CHECK(red >= 2 * 12 && yellow >= 2 * 20 && green >= 2 * 28);
        // First left-line piece starts at the trailer's rear-left corner + clearance.
        UnitGeometry g = rig.units[1];
        g.half_width += o.clearance;
        const Vec2 corner = body_corners(g, rig.poses[1])[3];
        CHECK(length(guides.front().from - corner) < 1e-9);
    }

    // Resampling keeps spacing on curved paths.
    {
        const Rig rig = build_rig(fixtures::single(0.25, 0.0));
        const auto guides = build_guides(rig, predict_reverse(rig, 12.0));
        for (const auto &s : guides) CHECK(length(s.to - s.from) <= 0.25 + 1e-6);
    }

    std::printf("core_tests: %d checks passed\n", checks);
    return 0;
}
