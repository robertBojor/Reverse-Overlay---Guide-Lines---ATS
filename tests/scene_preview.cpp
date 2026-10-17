// Renders representative scenarios to SVG so the overlay can be reviewed
// without the game. Uses the same panel layout rules as the DX11 renderer.
//   scene_preview <output-directory>

#include <cstdio>
#include <fstream>
#include <string>

#include "../src/core/panel_layout.hpp"
#include "../src/core/scene.hpp"
#include "fixtures.hpp"

using namespace rto;

static std::string rgba(Color c)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "rgba(%d,%d,%d,%.3f)", c.r, c.g, c.b, c.a / 255.0);
    return buffer;
}

static void write_svg(const std::string &path, const std::string &title, const Scene &scene)
{
    const float panel_w = 420.0f, panel_h = 560.0f, scale_px = 1.0f;
    PanelView view = fit_panel(scene, {0.0f, 0.0f, panel_w, panel_h}, scale_px);
    std::ofstream out(path);
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << panel_w << "' height='" << panel_h
        << "' font-family='Segoe UI, sans-serif'>\n";
    out << "<rect x='0' y='0' width='" << panel_w << "' height='" << panel_h
        << "' rx='10' fill='rgba(14,16,20,0.78)' stroke='rgba(255,255,255,0.18)'/>\n";
    // Grid.
    for (const auto &line : grid_lines(view, {0.0f, 0.0f, panel_w, panel_h}))
        out << "<line x1='" << line.a.x << "' y1='" << line.a.y << "' x2='" << line.b.x << "' y2='"
            << line.b.y << "' stroke='" << rgba(palette::grid) << "'/>\n";
    for (const auto &shape : scene.shapes)
    {
        std::string pts;
        for (const auto &p : shape.points)
        {
            const auto s = view.to_screen(p);
            pts += std::to_string(s.x) + "," + std::to_string(s.y) + " ";
        }
        if (shape.kind == Shape::Kind::convex_fill)
            out << "<polygon points='" << pts << "' fill='" << rgba(shape.color) << "'/>\n";
        else
            out << "<" << (shape.closed ? "polygon" : "polyline") << " points='" << pts
                << "' fill='none' stroke='" << rgba(shape.color) << "' stroke-width='"
                << shape.thickness * scale_px << "' stroke-linejoin='round'/>\n";
    }
    for (const auto &label : scene.labels)
    {
        const auto s = view.to_screen(label.at);
        out << "<text x='" << s.x << "' y='" << s.y + 4 << "' font-size='12' fill='"
            << rgba(label.color) << "'>" << label.text << "</text>\n";
    }
    float y = 22.0f;
    out << "<text x='14' y='" << y << "' font-size='13' font-weight='700' fill='"
        << rgba(palette::text) << "'>REVERSE</text>\n";
    for (const auto &line : scene.status)
    {
        y += 18.0f;
        const bool warn = line.rfind("JACKKNIFE", 0) == 0;
        out << "<text x='14' y='" << y << "' font-size='13' fill='"
            << rgba(warn ? palette::danger : palette::text) << "'>" << line << "</text>\n";
    }
    out << "<text x='" << panel_w - 14 << "' y='" << panel_h - 12
        << "' font-size='11' text-anchor='end' fill='rgba(255,255,255,0.45)'>" << title
        << "</text>\n";
    out << "</svg>\n";
}

int main(int argc, char **argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    struct Case
    {
        const char *file;
        const char *title;
        RigInput input;
        double horizon;
    };
    const double d = kPi / 180.0;
    const Case cases[] = {
        {"01_straight.svg", "straight, trailer aligned", fixtures::single(0.0, 0.0), 15.0},
        {"02_steer_left.svg", "wheels 12\xC2\xB0 left", fixtures::single(12 * d, 0.0), 15.0},
        {"03_articulated.svg", "trailer 25\xC2\xB0 tail right, wheels straight", fixtures::single(0.0, 25 * d), 15.0},
        {"04_correcting.svg", "trailer 25\xC2\xB0 tail left, wheels 20\xC2\xB0 left", fixtures::single(20 * d, -25 * d), 15.0},
        {"05_jackknife.svg", "wheels 30\xC2\xB0 left", fixtures::single(30 * d, 10 * d), 15.0},
        {"06_bobtail.svg", "bobtail, wheels 25\xC2\xB0 right", fixtures::bobtail(-25 * d), 12.0},
        {"07_doubles.svg", "28 ft doubles, wheels 6\xC2\xB0 right", fixtures::doubles(-6 * d, 0.0, 0.0), 12.0},
    };
    for (const auto &c : cases)
    {
        const Rig rig = build_rig(c.input);
        const Scene scene = build_scene(rig, predict_reverse(rig, c.horizon));
        write_svg(dir + "/" + c.file, c.title, scene);
        std::printf("wrote %s\n", c.file);
    }
    return 0;
}
