// SCS telemetry plugin entry points. Collects official SDK telemetry for the
// truck and up to ten trailers, predicts the reverse path every frame and
// hands the resulting scene to the DX11 overlay.

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <scssdk_telemetry.h>
#include <common/scssdk_telemetry_common_configs.h>
#include <common/scssdk_telemetry_trailer_common_channels.h>
#include <common/scssdk_telemetry_truck_common_channels.h>

#include "core/guides.hpp"
#include "core/rig.hpp"
#include "core/scene.hpp"
#include "plugin/config.hpp"
#include "plugin/log.hpp"
#include "plugin/engine.hpp"
#include "plugin/overlay_dx11.hpp"
#include "plugin/world_lines.hpp"

#include <MinHook.h>

namespace
{
// The game rejects wheel channel indices at or above these limits.
constexpr std::size_t truck_wheel_channels = 14;
constexpr std::size_t trailer_wheel_channels = 20;
constexpr std::size_t max_wheels = 24;
constexpr std::size_t max_trailers = SCS_TELEMETRY_trailers_count; // 10
constexpr std::size_t unit_count = 1 + max_trailers;              // 0 = truck

struct Unit
{
    // Configuration.
    bool configured = false;
    std::string id;
    std::size_t wheel_count = 0;
    std::array<rto::Vec2, max_wheels> wheel_pos{};
    std::array<bool, max_wheels> wheel_pos_valid{};
    std::array<bool, max_wheels> steerable{};
    std::array<bool, max_wheels> liftable{};
    std::optional<rto::Vec2> hook;

    // Channels.
    bool placement_valid = false;
    scs_value_dplacement_t placement{};
    bool connected = false; // trailers only
    std::array<bool, max_wheels> on_ground{};
    std::array<bool, max_wheels> on_ground_valid{};
    std::array<float, max_wheels> lift{};
    std::array<bool, max_wheels> lift_valid{};
    std::array<float, max_wheels> steering{};
    std::array<bool, max_wheels> steering_valid{};

    void reset_runtime()
    {
        placement_valid = false;
        connected = false;
        on_ground_valid.fill(false);
        lift_valid.fill(false);
        steering_valid.fill(false);
    }
};

enum Field : std::uintptr_t
{
    field_gear = 1,
    field_steering,
    field_placement,
    field_connected,
    field_wheel_on_ground,
    field_wheel_lift,
    field_wheel_steering,
};

// Channel context packs (field, unit, wheel) into the pointer value.
constexpr std::uintptr_t make_context(Field field, std::size_t unit, std::size_t wheel = 0)
{
    return (static_cast<std::uintptr_t>(field) << 16) | (unit << 8) | wheel;
}

struct State
{
    std::array<Unit, unit_count> units;
    int gear = 0;
    bool gear_valid = false;
    float steering_input = 0.0f;
    bool driving = false; // between "started" and "paused"
    bool seen_indexed_trailer_config = false;

    rto::Config config;
    std::filesystem::path config_path;
    std::filesystem::file_time_type config_time{};
    ULONGLONG next_config_check = 0;
    bool logged_first_rig = false;
    bool world_ready = false;   // engine functions found and render hook installed
    bool overlay_ready = false; // top-down panel hooks installed
};

State state;
std::deque<std::string> channel_names; // stable storage for registered names

std::filesystem::path documents_folder()
{
    PWSTR raw = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &raw)) && raw)
        result = raw;
    CoTaskMemFree(raw);
    return result;
}

void reload_config_if_changed(bool force)
{
    const ULONGLONG now = GetTickCount64();
    if (!force && now < state.next_config_check) return;
    state.next_config_check = now + 2000;
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(state.config_path, ec);
    if (!force && !ec && time == state.config_time) return;
    std::string report;
    state.config = rto::load_config(state.config_path, &report);
    state.config_time = std::filesystem::last_write_time(state.config_path, ec);
    rto::overlay::set_config(state.config);
    rto::log::info(report);
}

// ---------------------------------------------------------------------------
// Rig assembly

rto::Pose to_pose(const scs_value_dplacement_t &placement)
{
    return {{placement.position.x, placement.position.z},
            placement.orientation.heading * 2.0 * rto::kPi};
}

rto::UnitInput to_unit_input(const Unit &unit, bool *steering_valid)
{
    rto::UnitInput input;
    input.placement = to_pose(unit.placement);
    input.hook = unit.hook;
    for (std::size_t i = 0; i < unit.wheel_count && i < max_wheels; ++i)
    {
        if (!unit.wheel_pos_valid[i]) continue;
        rto::WheelInput w;
        w.pos = unit.wheel_pos[i];
        w.steerable = unit.steerable[i];
        w.liftable = unit.liftable[i];
        // Contact is authoritative; lift state is the fallback.
        if (unit.on_ground_valid[i])
            w.grounded = unit.on_ground[i];
        else if (unit.liftable[i] && unit.lift_valid[i])
            w.grounded = unit.lift[i] < 0.5f;
        if (unit.steering_valid[i])
        {
            w.steering = unit.steering[i] * 2.0 * rto::kPi; // SDK: rotations, CCW
            if (w.steerable && steering_valid) *steering_valid = true;
        }
        input.wheels.push_back(w);
    }
    return input;
}

// Height of the ground plane under a world point, from the rear unit's
// placement and attitude (pitch raises the front, roll raises the right side).
double ground_height(const scs_value_dplacement_t &anchor, rto::Vec2 world, double offset)
{
    constexpr double max_slope = 35.0 * rto::kPi / 180.0;
    const double pitch = std::clamp(anchor.orientation.pitch * 2.0 * rto::kPi, -max_slope, max_slope);
    const double roll = std::clamp(anchor.orientation.roll * 2.0 * rto::kPi, -max_slope, max_slope);
    const rto::Vec2 local = rto::to_local(to_pose(anchor), world);
    return anchor.position.y + offset + local.x * std::tan(roll) - local.z * std::tan(pitch);
}

void publish_world(const rto::Rig &rig, const rto::Prediction &prediction)
{
    if (!state.world_ready || !state.config.world_enabled)
    {
        rto::world::publish({});
        return;
    }
    rto::GuideOptions options;
    options.red_until = state.config.red_until_m;
    options.yellow_until = std::max(state.config.yellow_until_m, state.config.red_until_m);
    const auto segments = rto::build_guides(rig, prediction, options);
    const auto &anchor = state.units[rig.units.size() - 1].placement;
    const double offset = state.config.line_height_m;
    std::vector<rto::world::Piece> pieces;
    pieces.reserve(segments.size());
    for (const auto &s : segments)
    {
        const double y0 = ground_height(anchor, s.from, offset);
        const double y1 = ground_height(anchor, s.to, offset);
        pieces.push_back({rto::world::piece_transform(s.from.x, y0, s.from.z, s.to.x, y1, s.to.z),
                          s.band});
    }
    rto::world::publish(std::move(pieces));
}

void update_frame()
{
    reload_config_if_changed(false);

    const Unit &truck = state.units[0];
    const bool reversing = state.gear_valid && state.gear < 0;
    if (!state.driving || !truck.placement_valid || !truck.configured ||
        !(reversing || state.config.always_show))
    {
        rto::overlay::publish({});
        rto::world::publish({});
        return;
    }

    rto::RigInput input;
    bool steering_valid = false;
    input.truck = to_unit_input(truck, &steering_valid);
    input.wheel_steering_valid = steering_valid;
    input.steering_input = state.steering_input;
    for (std::size_t t = 1; t < unit_count; ++t)
    {
        const Unit &trailer = state.units[t];
        if (!trailer.connected || !trailer.placement_valid || !trailer.configured ||
            trailer.wheel_count == 0)
            break;
        input.trailers.push_back(to_unit_input(trailer, nullptr));
    }

    const rto::Rig rig = rto::build_rig(input);
    const rto::Prediction prediction = rto::predict_reverse(rig, state.config.horizon_m);
    if (state.overlay_ready) rto::overlay::publish(rto::build_scene(rig, prediction));
    if (reversing)
        publish_world(rig, prediction);
    else
        rto::world::publish({});

    if (!state.logged_first_rig)
    {
        state.logged_first_rig = true;
        const auto &g = rig.units[0];
        char message[256];
        std::snprintf(message, sizeof(message),
                      "First reverse frame: truck '%s' wheels=%zu wheelbase=%.2f m "
                      "per-wheel steering=%s trailers=%zu",
                      truck.id.c_str(), g.wheels.size(), g.wheelbase,
                      steering_valid ? "yes" : "no (fallback)", input.trailers.size());
        rto::log::info(message, true);
        for (std::size_t u = 1; u < rig.units.size(); ++u)
        {
            const auto &tg = rig.units[u];
            std::snprintf(message, sizeof(message),
                          "  trailer %zu '%s': wheels=%zu kingpin->axle=%.2f m length~%.2f m "
                          "coupling=(%.2f, %.2f)",
                          u, state.units[u].id.c_str(), tg.wheels.size(), tg.axle.z - tg.kingpin.z,
                          tg.rear_z - tg.front_z, tg.coupling_on_parent.x,
                          tg.coupling_on_parent.z);
            rto::log::info(message, true);
        }
    }
}

// ---------------------------------------------------------------------------
// Configuration events

int trailer_config_index(const char *id)
{
    // "trailer.N" (SDK 1.14+); legacy "trailer" mirrors trailer.0.
    if (std::strncmp(id, "trailer.", 8) == 0)
    {
        char *end = nullptr;
        const long n = std::strtol(id + 8, &end, 10);
        if (end != id + 8 && *end == '\0' && n >= 0 && n < static_cast<long>(max_trailers))
            return static_cast<int>(n);
        return -1;
    }
    if (std::strcmp(id, SCS_TELEMETRY_CONFIG_trailer) == 0) return -2;
    return -1;
}

void handle_configuration(const scs_telemetry_configuration_t *config)
{
    if (!config || !config->id) return;
    std::size_t unit_index;
    if (std::strcmp(config->id, SCS_TELEMETRY_CONFIG_truck) == 0)
        unit_index = 0;
    else
    {
        const int t = trailer_config_index(config->id);
        if (t == -1) return;
        if (t == -2)
        {
            if (state.seen_indexed_trailer_config) return;
            unit_index = 1;
        }
        else
        {
            state.seen_indexed_trailer_config = true;
            unit_index = 1 + static_cast<std::size_t>(t);
        }
    }

    Unit &unit = state.units[unit_index];
    Unit next;
    // Keep live channel data; replace configuration only.
    next.placement_valid = unit.placement_valid;
    next.placement = unit.placement;
    next.connected = unit.connected;
    next.on_ground = unit.on_ground;
    next.on_ground_valid = unit.on_ground_valid;
    next.lift = unit.lift;
    next.lift_valid = unit.lift_valid;
    next.steering = unit.steering;
    next.steering_valid = unit.steering_valid;

    for (const scs_named_value_t *a = config->attributes; a && a->name; ++a)
    {
        const std::string_view name = a->name;
        const auto index = static_cast<std::size_t>(a->index);
        if (name == SCS_TELEMETRY_CONFIG_ATTRIBUTE_id && a->value.type == SCS_VALUE_TYPE_string &&
            a->value.value_string.value)
            next.id = a->value.value_string.value;
        else if (name == SCS_TELEMETRY_CONFIG_ATTRIBUTE_hook_position &&
                 a->value.type == SCS_VALUE_TYPE_fvector)
            next.hook = rto::Vec2{a->value.value_fvector.x, a->value.value_fvector.z};
        else if (name == SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_count &&
                 a->value.type == SCS_VALUE_TYPE_u32)
            next.wheel_count = std::min<std::size_t>(a->value.value_u32.value, max_wheels);
        else if (index < max_wheels)
        {
            if (name == SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_position &&
                a->value.type == SCS_VALUE_TYPE_fvector)
            {
                next.wheel_pos[index] = {a->value.value_fvector.x, a->value.value_fvector.z};
                next.wheel_pos_valid[index] = true;
                next.wheel_count = std::max(next.wheel_count, index + 1);
            }
            else if (name == SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_steerable &&
                     a->value.type == SCS_VALUE_TYPE_bool)
                next.steerable[index] = a->value.value_bool.value != 0;
            else if (name == SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_liftable &&
                     a->value.type == SCS_VALUE_TYPE_bool)
                next.liftable[index] = a->value.value_bool.value != 0;
        }
    }
    next.configured = next.wheel_count > 0;
    unit = next;
    state.logged_first_rig = false;

    char message[200];
    std::snprintf(message, sizeof(message), "Configuration %s: id='%s' wheels=%zu hook=%s",
                  config->id, unit.id.c_str(), unit.wheel_count, unit.hook ? "yes" : "no");
    rto::log::info(message);
}

// ---------------------------------------------------------------------------
// Callbacks

SCSAPI_VOID channel_callback(const scs_string_t, const scs_u32_t, const scs_value_t *const value,
                             const scs_context_t context)
{
    const auto packed = reinterpret_cast<std::uintptr_t>(context);
    const auto field = static_cast<Field>(packed >> 16);
    const std::size_t unit_index = (packed >> 8) & 0xff;
    const std::size_t wheel = packed & 0xff;
    if (unit_index >= unit_count || wheel >= max_wheels) return;
    Unit &unit = state.units[unit_index];

    switch (field)
    {
    case field_gear:
        state.gear_valid = value != nullptr;
        state.gear = value ? value->value_s32.value : 0;
        break;
    case field_steering:
        state.steering_input = value ? value->value_float.value : 0.0f;
        break;
    case field_placement:
        unit.placement_valid = value != nullptr;
        if (value) unit.placement = value->value_dplacement;
        break;
    case field_connected:
        unit.connected = value && value->value_bool.value != 0;
        break;
    case field_wheel_on_ground:
        unit.on_ground_valid[wheel] = value != nullptr;
        if (value) unit.on_ground[wheel] = value->value_bool.value != 0;
        break;
    case field_wheel_lift:
        unit.lift_valid[wheel] = value != nullptr;
        if (value) unit.lift[wheel] = value->value_float.value;
        break;
    case field_wheel_steering:
        unit.steering_valid[wheel] = value != nullptr;
        if (value) unit.steering[wheel] = value->value_float.value;
        break;
    }
}

SCSAPI_VOID event_callback(const scs_event_t event, const void *const info, const scs_context_t)
{
    switch (event)
    {
    case SCS_TELEMETRY_EVENT_frame_end:
        update_frame();
        break;
    case SCS_TELEMETRY_EVENT_started:
        state.driving = true;
        rto::world::resume();
        break;
    case SCS_TELEMETRY_EVENT_paused:
        state.driving = false;
        rto::overlay::publish({});
        // Pause also covers menus and loading; free our model instances
        // before the game tears the scene down.
        rto::world::release_models();
        break;
    case SCS_TELEMETRY_EVENT_configuration:
        handle_configuration(static_cast<const scs_telemetry_configuration_t *>(info));
        break;
    default:
        break;
    }
}

const char *stable(std::string name)
{
    channel_names.push_back(std::move(name));
    return channel_names.back().c_str();
}

// "trailer.connected" -> "trailer.N.connected"
std::string trailer_channel(std::size_t index, const char *legacy_name)
{
    const std::string_view legacy = legacy_name;
    return "trailer." + std::to_string(index) + "." + std::string(legacy.substr(8));
}

int register_channels(const scs_telemetry_init_params_v101_t *api)
{
    int failures = 0;
    const auto reg = [&](const char *name, scs_u32_t index, scs_value_type_t type,
                         std::uintptr_t context) {
        // No each_frame flag: callbacks fire only when values change.
        const scs_result_t r = api->register_for_channel(
            name, index, type, SCS_TELEMETRY_CHANNEL_FLAG_no_value, channel_callback,
            reinterpret_cast<scs_context_t>(context));
        if (r != SCS_RESULT_ok) ++failures;
    };
    reg(SCS_TELEMETRY_TRUCK_CHANNEL_displayed_gear, SCS_U32_NIL, SCS_VALUE_TYPE_s32,
        make_context(field_gear, 0));
    reg(SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering, SCS_U32_NIL, SCS_VALUE_TYPE_float,
        make_context(field_steering, 0));
    reg(SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL, SCS_VALUE_TYPE_dplacement,
        make_context(field_placement, 0));
    for (scs_u32_t w = 0; w < truck_wheel_channels; ++w)
    {
        reg(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_on_ground, w, SCS_VALUE_TYPE_bool,
            make_context(field_wheel_on_ground, 0, w));
        reg(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_lift, w, SCS_VALUE_TYPE_float,
            make_context(field_wheel_lift, 0, w));
        reg(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_steering, w, SCS_VALUE_TYPE_float,
            make_context(field_wheel_steering, 0, w));
    }
    for (std::size_t t = 0; t < max_trailers; ++t)
    {
        const std::size_t unit = 1 + t;
        reg(stable(trailer_channel(t, SCS_TELEMETRY_TRAILER_CHANNEL_connected)), SCS_U32_NIL,
            SCS_VALUE_TYPE_bool, make_context(field_connected, unit));
        reg(stable(trailer_channel(t, SCS_TELEMETRY_TRAILER_CHANNEL_world_placement)), SCS_U32_NIL,
            SCS_VALUE_TYPE_dplacement, make_context(field_placement, unit));
        const char *on_ground = stable(trailer_channel(t, SCS_TELEMETRY_TRAILER_CHANNEL_wheel_on_ground));
        const char *lift = stable(trailer_channel(t, SCS_TELEMETRY_TRAILER_CHANNEL_wheel_lift));
        const char *steer = stable(trailer_channel(t, SCS_TELEMETRY_TRAILER_CHANNEL_wheel_steering));
        for (scs_u32_t w = 0; w < trailer_wheel_channels; ++w)
        {
            reg(on_ground, w, SCS_VALUE_TYPE_bool, make_context(field_wheel_on_ground, unit, w));
            reg(lift, w, SCS_VALUE_TYPE_float, make_context(field_wheel_lift, unit, w));
            reg(steer, w, SCS_VALUE_TYPE_float, make_context(field_wheel_steering, unit, w));
        }
    }
    return failures;
}

void game_log_bridge(int type, const char *message);
scs_log_t sdk_log = nullptr;
void game_log_bridge(int type, const char *message)
{
    if (sdk_log) sdk_log(static_cast<scs_log_type_t>(type), message);
}
}

SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t *const params)
{
    if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
    const auto *api = static_cast<const scs_telemetry_init_params_v101_t *>(params);
    sdk_log = api->common.log;

    const auto folder = documents_folder() / "American Truck Simulator";
    rto::log::init(folder / "ats_reverse_overlay.log", &game_log_bridge);
    char message[256];
    std::snprintf(message, sizeof(message), "ATS Reverse Overlay starting in %s %u.%u",
                  api->common.game_name ? api->common.game_name : "?",
                  SCS_GET_MAJOR_VERSION(api->common.game_version),
                  SCS_GET_MINOR_VERSION(api->common.game_version));
    rto::log::info(message, true);

    state.config_path = folder / "ats_reverse_overlay.ini";
    reload_config_if_changed(true);

    const bool events =
        api->register_for_event(SCS_TELEMETRY_EVENT_frame_end, event_callback, nullptr) == SCS_RESULT_ok &&
        api->register_for_event(SCS_TELEMETRY_EVENT_started, event_callback, nullptr) == SCS_RESULT_ok &&
        api->register_for_event(SCS_TELEMETRY_EVENT_paused, event_callback, nullptr) == SCS_RESULT_ok &&
        api->register_for_event(SCS_TELEMETRY_EVENT_configuration, event_callback, nullptr) == SCS_RESULT_ok;
    if (!events)
    {
        rto::log::error("Could not register telemetry events.", true);
        return SCS_RESULT_generic_error;
    }
    const int failures = register_channels(api);
    if (failures)
        rto::log::warn("Some telemetry channels were not available: " + std::to_string(failures));

    if (MH_Initialize() != MH_OK)
    {
        rto::log::error("MinHook initialisation failed; telemetry only.", true);
        return SCS_RESULT_ok;
    }

    std::string error;
    if (!state.config.enabled)
        rto::log::info("Top-down panel disabled in settings; DirectX not hooked.");
    else if (rto::overlay::install(&error))
    {
        state.overlay_ready = true;
        rto::log::info("Overlay hooks installed (DXGI Present).", true);
    }
    else
        rto::log::error("Overlay unavailable: " + error, true);

    // In-world guide lines: engine functions are located by pattern; any
    // mismatch leaves the top-down panel as the only display.
    rto::engine::Functions functions;
    const auto resolution = rto::engine::resolve(&functions);
    rto::log::info(resolution.report, true);
    if (!resolution.ok)
        rto::log::warn("In-world guide lines unavailable on this game build.", true);
    else if (!resolution.verified_build && !state.config.world_allow_unverified)
        rto::log::warn("In-world guide lines skipped: build not verified and allow_unverified = 0.", true);
    else if (!rto::world::install(functions, &error))
        rto::log::error("In-world guide lines unavailable: " + error, true);
    else
    {
        state.world_ready = true;
        rto::log::info("In-world guide lines ready (vehicle render hook installed).", true);
    }
    return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void)
{
    rto::log::info("Shutting down.");
    rto::world::release_models();
    const bool world_idle = rto::world::uninstall();
    const bool overlay_idle = rto::overlay::uninstall();
    if (world_idle && overlay_idle)
        MH_Uninitialize();
    else
        rto::log::warn("A thread was still inside a hook at shutdown; leaving MinHook trampolines allocated.");
    rto::log::shutdown();
    sdk_log = nullptr;
}
