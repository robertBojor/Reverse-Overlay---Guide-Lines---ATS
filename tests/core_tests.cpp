#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>

#include "../src/core/display_mode.hpp"
#include "../src/core/forward.hpp"
#include "../src/core/guides.hpp"
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
    // stops there.
    {
        const Rig rig = build_rig(fixtures::single(0.35, 0.0));
        const Prediction p = predict_reverse(rig, 60.0);
        CHECK(p.jackknife_distance > 2.0);
        CHECK(p.jackknife_unit == 1);
        CHECK(std::abs(deg(articulation(p.steps.back().poses[0], p.steps.back().poses[1]))) > 84.0);
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

    // Forward line length: speed x time, within the limits.
    {
        ForwardOptions o;
        o.min_length = 25.0;
        o.length_time = 1.2;
        o.max_length = 40.0;
        CHECK(near(forward_length(0.0, o), 25.0, 1e-12));
        CHECK(near(forward_length(-3.0, o), 25.0, 1e-12)); // rolling back in a forward gear
        CHECK(near(forward_length(25.0, o), 30.0, 1e-12));
        CHECK(near(forward_length(100.0, o), 40.0, 1e-12));
    }

    // Curvature: wheel angles when slow, the measured turn rate at speed,
    // blended in between.
    {
        ForwardOptions o;
        o.measured_from = 5.0;
        o.measured_full = 10.0;
        CHECK(near(forward_curvature(0.02, 0.0, 0.0, o), 0.02, 1e-12));
        CHECK(near(forward_curvature(0.02, 0.5, 5.0, o), 0.02, 1e-12));
        CHECK(near(forward_curvature(0.02, 0.1, 25.0, o), 0.004, 1e-12)); // understeer at speed
        CHECK(near(forward_curvature(0.0, 0.08, 7.5, o), 0.08 / 7.5 * 0.5, 1e-12));
        CHECK(near(forward_curvature(0.01, -0.25, 25.0, o), -0.01, 1e-12)); // turning right
    }

    // The front axle sits ahead of the rear axle by the wheelbase.
    {
        const Rig rig = build_rig(fixtures::bobtail(0.0));
        const auto &g = rig.units[0];
        CHECK(near(g.front_axle.x, 0.0, 1e-12));
        CHECK(near(g.axle.z - g.front_axle.z, g.wheelbase, 1e-9));
        CHECK(g.front_axle.z > g.front_z); // behind the bumper
    }

    // Straight ahead: one blue line from the front axle, straight forward,
    // cut into equal pieces.
    {
        const Rig rig = build_rig(fixtures::bobtail(0.0));
        const auto &g = rig.units[0];
        ForwardOptions o;
        o.width = o.piece_width;
        const auto line = build_forward(rig, 24.4, o);
        CHECK(line.size() == 24); // whole 1 m pieces; the 0.4 m tail is dropped
        double total = 0.0, min_z = 1e9, max_z = -1e9;
        for (const auto &s : line)
        {
            CHECK(s.band == Band::blue);
            const Vec2 a = to_local(rig.poses[0], s.from), b = to_local(rig.poses[0], s.to);
            CHECK(near(a.x, 0.0, 1e-9) && near(b.x, 0.0, 1e-9));
            min_z = std::min({min_z, a.z, b.z});
            max_z = std::max({max_z, a.z, b.z});
            total += length(s.to - s.from);
        }
        CHECK(near(total, 24.0, 1e-6));
        for (const auto &s : line) CHECK(near(length(s.to - s.from), 1.0, 1e-9)); // edge to edge
        CHECK(near(max_z, g.front_axle.z, 1e-9) && near(min_z, g.front_axle.z - 24.0, 1e-6));
    }

    // A wider line is parallel strips centred on the path.
    {
        const Rig rig = build_rig(fixtures::bobtail(0.0));
        ForwardOptions o;
        o.width = 0.3;
        const auto line = build_forward(rig, 24.0, o);
        CHECK(line.size() == 24 * 2);
        double min_x = 1e9, max_x = -1e9;
        for (const auto &s : line)
        {
            const double x = to_local(rig.poses[0], s.from).x;
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
        }
        CHECK(near(max_x - min_x, o.piece_width, 1e-9)); // strips side by side, no overlap
        CHECK(near(min_x + max_x, 0.0, 1e-9));
        o.width = 0.4; // rounds to whole strips
        CHECK(build_forward(rig, 24.0, o).size() == 24 * 3);
    }

    // Steering left: the front axle turns about the same centre as the rear
    // axle, so the line is an arc of radius sqrt(R^2 + wheelbase^2), bending
    // left from the truck.
    {
        const Rig rig = build_rig(fixtures::bobtail(0.3));
        const auto &g = rig.units[0];
        ForwardOptions o;
        o.width = o.piece_width;
        const double R = 1.0 / g.curvature;
        const Vec2 centre = to_world(rig.poses[0], {-R, g.axle.z});
        const double r = length(to_world(rig.poses[0], g.front_axle) - centre);
        CHECK(near(r, std::hypot(R, g.wheelbase), 1e-9));
        const auto line = build_forward(rig, 10.0, o);
        CHECK(!line.empty());
        for (const auto &s : line)
            CHECK(near(length(s.from - centre), r, 1e-3) && near(length(s.to - centre), r, 1e-3));
        CHECK(to_local(rig.poses[0], line.back().to).x < -0.5);
    }

    // On a tight turn the line stops after a quarter turn instead of curling round.
    {
        const Rig rig = build_rig(fixtures::bobtail(0.6));
        const auto &g = rig.units[0];
        ForwardOptions o;
        o.width = o.piece_width;
        const double R = 1.0 / g.curvature;
        CHECK(60.0 > o.max_turn * R); // the full length would turn further
        const auto line = build_forward(rig, 60.0, o);
        const Vec2 centre = to_world(rig.poses[0], {-R, g.axle.z});
        const Vec2 start = to_world(rig.poses[0], g.front_axle) - centre;
        const Vec2 end = line.back().to - centre;
        const double turned = std::acos(dot(start, end) / (length(start) * length(end)));
        CHECK(turned <= o.max_turn + 0.05 && turned > o.max_turn - 0.1);
        CHECK(build_forward(rig, 0.0, o).empty());
    }

    // Trailer line, straight: the trailer follows the truck, nothing to show.
    {
        const Rig rig = build_rig(fixtures::single(0.0, 0.0));
        ForwardOptions o;
        o.width = o.piece_width;
        const auto t = build_trailer_forward(rig, 30.0, o);
        CHECK(t.offtrack < 1e-6);
        CHECK(!t.segments.empty());
        for (const auto &s : t.segments) CHECK(s.band == Band::teal);
        CHECK(build_trailer_forward(build_rig(fixtures::bobtail(0.2)), 30.0, o).segments.empty());
    }

    // Trailer line on a steady left turn: the trailer axle settles on radius
    // sqrt(Rk^2 - d^2) about the turn centre (Rk: kingpin radius, d: kingpin to
    // axle), so it runs inside the front axle's circle by the difference. The
    // line follows the inner (left) wheels.
    {
        const Rig rig = build_rig(fixtures::single(0.1, 0.0));
        const auto &tr = rig.units[0];
        const auto &tl = rig.units[1];
        ForwardOptions o;
        o.width = o.piece_width;
        const double R = 1.0 / tr.curvature;
        const Vec2 centre = to_world(rig.poses[0], {-R, tr.axle.z});
        const double r_front = std::hypot(R, tr.wheelbase);
        const double r_kingpin = length(to_world(rig.poses[0], tl.coupling_on_parent) - centre);
        const double d = tl.axle.z - tl.kingpin.z;
        const double r_trailer = std::sqrt(r_kingpin * r_kingpin - d * d);
        const double span = 200.0; // capped at a quarter turn, about 90 m: settled
        const auto t = build_trailer_forward(rig, span, o);
        CHECK(near(t.offtrack, r_front - r_trailer, 0.05));
        CHECK(t.offtrack > 1.0);
        const Prediction p = predict_motion(rig, std::min(span, o.max_turn * R), 1.0, 0.05, 0.2);
        CHECK(near(length(to_world(p.steps.back().poses[1], tl.axle) - centre), r_trailer, 0.02));
        const double r_line = length(t.segments.back().to - centre);
        CHECK(near(r_line, r_trailer - tl.half_width, 0.05)); // the inner wheels
        CHECK(to_local(rig.poses[0], t.segments.front().from).z < tr.front_axle.z + 1.0); // ahead
    }

    // Turning right: the line runs ahead of the truck, alongside the forward
    // line and inside it, along the trailer's right-hand wheels.
    {
        const Rig rig = build_rig(fixtures::single(-0.2, 0.0));
        const auto &tr = rig.units[0];
        ForwardOptions o;
        o.width = o.piece_width;
        const auto t = build_trailer_forward(rig, 25.0, o);
        CHECK(t.offtrack > 0.5);
        CHECK(!t.segments.empty());
        const double R = 1.0 / tr.curvature; // negative: centre on the right
        const Vec2 centre = to_world(rig.poses[0], {-R, tr.axle.z});
        const double r_front = std::hypot(R, tr.wheelbase);
        for (const auto &s : t.segments)
            CHECK(length(s.from - centre) < r_front - 0.5 && length(s.to - centre) < r_front - 0.5);
        // It starts level with the front axle, not back at the trailer.
        const Vec2 first = to_local(rig.poses[0], t.segments.front().from);
        CHECK(first.z < tr.front_axle.z + 1.0);
    }


    // reversing from the predicted pose returns to the start.
    {
        const Rig rig = build_rig(fixtures::bobtail(0.25, 0.7));
        const Pose end = predict_forward(rig.units[0], rig.poses[0], 12.0);
        Rig back = rig;
        back.poses[0] = end;
        const Pose home = predict_reverse(back, 12.0).steps.back().poses[0];
        CHECK(length(home.p - rig.poses[0].p) < 1e-4); // reverse uses 5 cm midpoint steps
        CHECK(near(normalize_angle(home.heading - rig.poses[0].heading), 0.0, 1e-9));
    }

    // Steering smoothing.
    {
        CHECK(near(smooth_toward(0.0, 1.0, 0.0, 0.25), 0.0, 1e-12));
        CHECK(near(smooth_toward(0.0, 1.0, 0.016, 0.0), 1.0, 1e-12));
        CHECK(near(smooth_toward(0.0, 1.0, 0.25, 0.25), 1.0 - std::exp(-1.0), 1e-12));
        CHECK(smooth_toward(0.0, 1.0, 10.0, 0.25) > 0.999);
    }

    // Hotkey cycle: forward only -> reverse only -> both -> off -> forward only.
    {
        DisplayMode m = DisplayMode::forward;
        CHECK(shows_forward(m) && !shows_reverse(m));
        m = next_mode(m);
        CHECK(m == DisplayMode::reverse && !shows_forward(m) && shows_reverse(m));
        m = next_mode(m);
        CHECK(m == DisplayMode::both && shows_forward(m) && shows_reverse(m));
        m = next_mode(m);
        CHECK(m == DisplayMode::off && !shows_forward(m) && !shows_reverse(m));
        CHECK(next_mode(m) == DisplayMode::forward);
        for (const auto mode : {DisplayMode::forward, DisplayMode::reverse, DisplayMode::both, DisplayMode::off})
            CHECK(parse_mode(mode_name(mode)) == mode);
        CHECK(!parse_mode("sideways"));
    }

    std::printf("core_tests: %d checks passed\n", checks);
    return 0;
}
