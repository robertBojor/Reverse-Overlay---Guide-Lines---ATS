#pragma once

// What the hotkey shows. Each press moves to the next mode:
// forward only -> reverse only -> both -> off -> forward only.
// "Both" means the forward line in a forward gear and guide lines in reverse.

#include <optional>
#include <string_view>

namespace rto
{
enum class DisplayMode
{
    forward,
    reverse,
    both,
    off,
};

inline DisplayMode next_mode(DisplayMode m)
{
    switch (m)
    {
    case DisplayMode::forward: return DisplayMode::reverse;
    case DisplayMode::reverse: return DisplayMode::both;
    case DisplayMode::both: return DisplayMode::off;
    case DisplayMode::off: break;
    }
    return DisplayMode::forward;
}

inline bool shows_forward(DisplayMode m) { return m == DisplayMode::forward || m == DisplayMode::both; }
inline bool shows_reverse(DisplayMode m) { return m == DisplayMode::reverse || m == DisplayMode::both; }

inline const char *mode_name(DisplayMode m)
{
    switch (m)
    {
    case DisplayMode::forward: return "forward";
    case DisplayMode::reverse: return "reverse";
    case DisplayMode::both: return "both";
    case DisplayMode::off: break;
    }
    return "off";
}

inline std::optional<DisplayMode> parse_mode(std::string_view s)
{
    for (const auto m : {DisplayMode::forward, DisplayMode::reverse, DisplayMode::both, DisplayMode::off})
        if (s == mode_name(m)) return m;
    return std::nullopt;
}
}
