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

[overlay]
; Top-down panel. 1 = on, 0 = off. Takes effect the next time the game starts
; (when off, the plugin does not hook DirectX at all).
enabled = 0
; Show the panel even when not in reverse (handy while positioning it)
always_show = 0
; left, right, top_left, top_right, bottom_left, bottom_right
anchor = right
; Panel height as a fraction of the screen height, and width/height ratio
panel_height = 0.42
aspect = 0.75
; Gap to the screen edge and extra offset, as fractions of the screen height
margin = 0.02
offset_x = 0.0
offset_y = 0.0
; 0.2 - 1.0
opacity = 0.9

[prediction]
; How far back to predict, in metres
horizon_m = 15

[world]
; Guide lines drawn on the ground behind the vehicle (also visible in mirrors).
; Needs the "Reverse Overlay - Guide Lines - ATS" mod enabled in the Mod Manager.
enabled = 1
; Colour bands by distance travelled: red, then yellow, then green
red_until_m = 3
yellow_until_m = 8
; Line height above the rear unit's reference point, in metres
line_height_m = 0.35
; Also run on game versions not yet confirmed working (0 = only confirmed builds)
allow_unverified = 1

[hotkey]
; Show/hide the top-down panel (does not affect the guide lines).
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

        if (id == "overlay.enabled") c.enabled = parse_bool(value);
        else if (id == "overlay.always_show") c.always_show = parse_bool(value);
        else if (id == "overlay.anchor")
        {
            if (value == "left") c.anchor = Anchor::left;
            else if (value == "right") c.anchor = Anchor::right;
            else if (value == "top_left") c.anchor = Anchor::top_left;
            else if (value == "top_right") c.anchor = Anchor::top_right;
            else if (value == "bottom_left") c.anchor = Anchor::bottom_left;
            else if (value == "bottom_right") c.anchor = Anchor::bottom_right;
            else problems << " unknown anchor '" << value << "';";
        }
        else if (id == "overlay.panel_height") c.panel_height = parse_float(value, c.panel_height, 0.15f, 0.95f);
        else if (id == "overlay.aspect") c.aspect = parse_float(value, c.aspect, 0.4f, 2.5f);
        else if (id == "overlay.margin") c.margin = parse_float(value, c.margin, 0.0f, 0.3f);
        else if (id == "overlay.offset_x") c.offset_x = parse_float(value, c.offset_x, -2.0f, 2.0f);
        else if (id == "overlay.offset_y") c.offset_y = parse_float(value, c.offset_y, -1.0f, 1.0f);
        else if (id == "overlay.opacity") c.opacity = parse_float(value, c.opacity, 0.2f, 1.0f);
        else if (id == "world.enabled") c.world_enabled = parse_bool(value);
        else if (id == "world.allow_unverified") c.world_allow_unverified = parse_bool(value);
        else if (id == "world.red_until_m") c.red_until_m = parse_float(value, c.red_until_m, 0.0f, 60.0f);
        else if (id == "world.yellow_until_m") c.yellow_until_m = parse_float(value, c.yellow_until_m, 0.0f, 60.0f);
        else if (id == "world.line_height_m") c.line_height_m = parse_float(value, c.line_height_m, -1.0f, 2.0f);
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
