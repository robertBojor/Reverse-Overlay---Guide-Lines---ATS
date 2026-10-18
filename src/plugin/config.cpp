#include "plugin/config.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace rto
{
namespace
{
constexpr const char *default_file = R"(; Reverse Overlay - Guide Lines - ATS settings. Most changes apply while the game runs.

[prediction]
; How far back to predict, in metres
horizon_m = 15

[world]
; Reverse guide lines and forward dots drawn on the ground (also visible in mirrors).
; Needs the "Reverse Overlay - Guide Lines - ATS" mod enabled in the Mod Manager.
enabled = 1
; Colour bands by distance travelled: red, then yellow, then green
red_until_m = 3
yellow_until_m = 8
; Line height above the rear unit's reference point, in metres
line_height_m = 0.35
; Also run on game versions not yet confirmed working (0 = only confirmed builds)
allow_unverified = 1

[forward]
; Two square dots ahead of the truck in a forward gear: red marks where the
; front-left corner will be, green the front-right corner. Truck only.
; Look-ahead time in seconds: the dots are speed x this far ahead
lookahead_s = 1.5
; Distance limits in metres (the minimum applies when slow or stopped)
min_distance_m = 6
max_distance_m = 80
; Dot edge length in metres (0.12 - 1.0)
dot_size_m = 0.12
; Dot height above the truck's reference point, in metres
dot_height_m = 0.35
; Steering smoothing in seconds, so the dots don't jitter at speed (0 = off)
steering_smoothing_s = 0.25

[hotkey]
; Each press moves to the next mode: forward dots only -> reverse lines only
; -> both (dots forward, lines in reverse) -> off. The mode is remembered.
; key is a letter, digit, F1-F12, or none
key = F9
ctrl = 1
shift = 1
alt = 0
)";

std::string trim(std::string s)
{
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool parse_bool(const std::string &v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }

float parse_float(const std::string &v, float fallback, float lo, float hi)
{
    char *end = nullptr;
    const float f = std::strtof(v.c_str(), &end);
    if (end == v.c_str()) return fallback;
    return std::clamp(f, lo, hi);
}

int parse_key(const std::string &v)
{
    const std::string k = lower(v);
    if (k.empty() || k == "none" || k == "0") return 0;
    if (k.size() == 1 && std::isalnum(static_cast<unsigned char>(k[0])))
        return std::toupper(static_cast<unsigned char>(k[0]));
    if (k.size() >= 2 && k[0] == 'f')
    {
        const int n = std::atoi(k.c_str() + 1);
        if (n >= 1 && n <= 12) return VK_F1 + n - 1;
    }
    return -1;
}
}

Config load_config(const std::filesystem::path &path, std::string *report)
{
    Config c;
    std::ifstream in(path);
    if (!in)
    {
        std::error_code ignored;
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream(path) << default_file;
        if (report) *report = "wrote default settings to " + path.string();
        return c;
    }

    std::ostringstream problems;
    std::string section, line;
    while (std::getline(in, line))
    {
        line = trim(line.substr(0, line.find_first_of(";#")));
        if (line.empty()) continue;
        if (line.front() == '[')
        {
            section = lower(trim(line.substr(1, line.find(']') - 1)));
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lower(trim(line.substr(0, eq)));
        const std::string value = lower(trim(line.substr(eq + 1)));
        const std::string id = section + "." + key;

        // [overlay] configured the top-down panel, removed after 0.1.0. Older
        // settings files still contain it; skip it without complaint.
        if (section == "overlay") continue;
        if (id == "world.enabled") c.world_enabled = parse_bool(value);
        else if (id == "world.allow_unverified") c.world_allow_unverified = parse_bool(value);
        else if (id == "world.red_until_m") c.red_until_m = parse_float(value, c.red_until_m, 0.0f, 60.0f);
        else if (id == "world.yellow_until_m") c.yellow_until_m = parse_float(value, c.yellow_until_m, 0.0f, 60.0f);
        else if (id == "world.line_height_m") c.line_height_m = parse_float(value, c.line_height_m, -1.0f, 2.0f);
        else if (id == "forward.lookahead_s") c.lookahead_s = parse_float(value, c.lookahead_s, 0.0f, 10.0f);
        else if (id == "forward.min_distance_m") c.min_distance_m = parse_float(value, c.min_distance_m, 1.0f, 50.0f);
        else if (id == "forward.max_distance_m") c.max_distance_m = parse_float(value, c.max_distance_m, 1.0f, 200.0f);
        else if (id == "forward.dot_size_m") c.dot_size_m = parse_float(value, c.dot_size_m, 0.12f, 1.0f);
        else if (id == "forward.dot_height_m") c.dot_height_m = parse_float(value, c.dot_height_m, -1.0f, 2.0f);
        else if (id == "forward.steering_smoothing_s") c.steering_smoothing_s = parse_float(value, c.steering_smoothing_s, 0.0f, 2.0f);
        else if (id == "prediction.horizon_m") c.horizon_m = parse_float(value, c.horizon_m, 3.0f, 60.0f);
        else if (id == "hotkey.key")
        {
            const int vk = parse_key(value);
            if (vk < 0) problems << " unknown hotkey '" << value << "';";
            else c.toggle_vk = vk;
        }
        else if (id == "hotkey.ctrl") c.toggle_ctrl = parse_bool(value);
        else if (id == "hotkey.shift") c.toggle_shift = parse_bool(value);
        else if (id == "hotkey.alt") c.toggle_alt = parse_bool(value);
        else problems << " unknown setting '" << id << "';";
    }
    if (report)
    {
        *report = "loaded settings from " + path.string();
        const std::string p = problems.str();
        if (!p.empty()) *report += " (ignored:" + p + ")";
    }
    return c;
}
}
