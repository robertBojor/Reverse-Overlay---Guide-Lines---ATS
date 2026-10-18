#pragma once

#include <filesystem>
#include <string>

namespace rto
{
struct Config
{
    float horizon_m = 15.0f;
    // In-world guide lines (needs ats_reverse_overlay.scs enabled).
    bool world_enabled = true;
    bool world_allow_unverified = true; // run on game builds not yet tested in game
    float red_until_m = 3.0f;
    float yellow_until_m = 8.0f;
    float line_height_m = 0.35f;        // above the rear unit's placement origin
    // Hotkey showing/hiding the guide lines. Virtual-key code plus modifiers; vk 0 disables.
    int toggle_vk = 0x78; // VK_F9; Ctrl+Shift+R opens the in-game radio
    bool toggle_ctrl = true;
    bool toggle_shift = true;
    bool toggle_alt = false;
};

// Reads `path`, writing a commented default file if it does not exist.
Config load_config(const std::filesystem::path &path, std::string *report = nullptr);
}
