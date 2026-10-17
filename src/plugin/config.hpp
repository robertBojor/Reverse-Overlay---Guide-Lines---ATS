#pragma once

#include <filesystem>
#include <string>

namespace rto
{
enum class Anchor
{
    left,
    right,
    top_left,
    top_right,
    bottom_left,
    bottom_right,
};

struct Config
{
    bool enabled = false;               // top-down panel (read at game start)
    bool always_show = false;   // show outside reverse gear (for positioning)
    Anchor anchor = Anchor::right;
    float panel_height = 0.42f; // fraction of screen height
    float aspect = 0.75f;       // panel width / height
    float margin = 0.02f;       // fraction of screen height
    float offset_x = 0.0f;      // fraction of screen height, +right
    float offset_y = 0.0f;      // fraction of screen height, +down
    float opacity = 0.9f;
    float horizon_m = 15.0f;
    // In-world guide lines (needs ats_reverse_overlay.scs enabled).
    bool world_enabled = true;
    bool world_allow_unverified = true; // run on game builds not yet tested in game
    float red_until_m = 3.0f;
    float yellow_until_m = 8.0f;
    float line_height_m = 0.35f;        // above the rear unit's placement origin
    // Hotkey toggling the overlay. Virtual-key code plus modifiers; vk 0 disables.
    int toggle_vk = 0x78; // VK_F9; Ctrl+Shift+R opens the in-game radio
    bool toggle_ctrl = true;
    bool toggle_shift = true;
    bool toggle_alt = false;
};

// Reads `path`, writing a commented default file if it does not exist.
Config load_config(const std::filesystem::path &path, std::string *report = nullptr);
}
