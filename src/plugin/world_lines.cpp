#include "plugin/world_lines.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>

#include <MinHook.h>

#include "plugin/log.hpp"

namespace rto::world
{
namespace
{
// Per model. The largest user is the forward line at the setting limits:
// 0.6 m wide (4 strips) and 100 m long in 1 m pieces, 400 pieces.
// Instances are created on first use, so unused slots cost a pointer.
constexpr std::size_t pool_size = 512;
// Indexed by Band.
constexpr std::array<const char *, 5> model_paths{
    "/model/rto_guides/line_red.pmd",
    "/model/rto_guides/line_yellow.pmd",
    "/model/rto_guides/line_green.pmd",
    "/model/rto_guides/line_blue.pmd",
    "/model/rto_guides/line_teal.pmd",
};

engine::Functions fn;
engine::VehicleRenderDispatch original_dispatch = nullptr;
bool hook_installed = false;
std::atomic<int> in_flight{0};

std::mutex pieces_mutex;
std::vector<Piece> latest;

// Guarded by models_mutex (render thread draws, game thread releases).
std::mutex models_mutex;
std::array<std::array<std::uint64_t *, pool_size>, model_paths.size()> pools{};
std::atomic<bool> suspended{false}; // between release_models() and resume()
std::atomic<bool> faulted{false};
// A model that failed to load (e.g. a mod older than the plugin, without the
// forward-line models) is skipped for the session; the other models keep drawing.
std::array<bool, model_paths.size()> missing{};
bool logged_first_render = false;

const engine::PrismTransform hidden_transform{0.0f, -10000.0f, 0.0f, 0, 0, 1.0f, 0.0f, 0.0f, 0.0f};

std::uint64_t node_of(std::uint64_t *model)
{
    return reinterpret_cast<std::uint64_t>(model) + engine::placement_node_offset;
}

// Engine sequence verified by the original mod: load, take ownership,
// initialise scene parameters, place, finalise render state.
std::uint64_t *create_model(const char *path, const engine::PrismTransform &initial)
{
    std::uint64_t output = 0, owned = 0;
    const std::uint64_t variant = 0, look = 0;
    fn.model_load(&output, &path, &variant, &look, 0);
    if (!output) return nullptr;
    fn.model_transfer(&owned, &output);
    if (!owned) return nullptr;
    auto *model = reinterpret_cast<std::uint64_t *>(owned);
    fn.model_parameter_init(model);
    fn.set_transform(node_of(model), &initial);
    auto **vtable = *reinterpret_cast<void ***>(model);
    if (!vtable || !vtable[engine::vtable_finalize_slot]) return nullptr;
    reinterpret_cast<void(__fastcall *)(std::uint64_t *)>(vtable[engine::vtable_finalize_slot])(model);
    return model;
}

void render_model(std::uint64_t *model, const engine::PrismTransform &t, std::uint64_t context)
{
    fn.set_transform(node_of(model), &t);
    auto **vtable = *reinterpret_cast<void ***>(model);
    reinterpret_cast<void(__fastcall *)(std::uint64_t *, std::uint64_t *)>(
        vtable[engine::vtable_render_slot])(model, reinterpret_cast<std::uint64_t *>(context));
}

void destroy_model(std::uint64_t *model)
{
    std::uint64_t stored = reinterpret_cast<std::uint64_t>(model), empty = 0;
    fn.model_transfer(&stored, &empty);
}

void render_pieces(std::uint64_t context)
{
    std::vector<Piece> pieces;
    {
        std::lock_guard lock(pieces_mutex);
        pieces = latest;
    }
    std::lock_guard lock(models_mutex);
    std::array<std::size_t, model_paths.size()> used{};
    for (const auto &piece : pieces)
    {
        const auto band = static_cast<std::size_t>(piece.band);
        auto &pool = pools[band];
        std::size_t &index = used[band];
        if (index >= pool.size() || missing[band]) continue;
        if (!pool[index])
        {
            pool[index] = create_model(model_paths[band], piece.transform);
            if (!pool[index])
            {
                log::error(std::string("Could not create ") + model_paths[band] +
                           " - is the \"Backing & Lane Prediction - ATS\" mod enabled in the Mod Manager,"
                           " and as new as the plugin? Skipping this model until the game restarts.");
                missing[band] = true;
                continue;
            }
        }
        render_model(pool[index], piece.transform, context);
        ++index;
    }
    // Park instances that are not drawn this pass.
    for (std::size_t band = 0; band < pools.size(); ++band)
        for (std::size_t i = used[band]; i < pools[band].size() && pools[band][i]; ++i)
            fn.set_transform(node_of(pools[band][i]), &hidden_transform);

    if (!pieces.empty() && !logged_first_render)
    {
        logged_first_render = true;
        log::info("In-world guide lines rendering: " + std::to_string(pieces.size()) + " pieces.");
    }
}

void __fastcall hooked_dispatch(std::uint64_t wrapper, std::uint64_t context)
{
    in_flight.fetch_add(1, std::memory_order_acq_rel);
    original_dispatch(wrapper, context);
    if (context && !suspended.load(std::memory_order_acquire) &&
        !faulted.load(std::memory_order_acquire))
    {
        // Built with /EHa: catch(...) also catches access violations and
        // unwinds the mutex locks above.
        try
        {
            render_pieces(context);
        }
        catch (...)
        {
            faulted = true;
            log::error("In-world rendering raised an exception; guide lines disabled for this session.");
        }
    }
    in_flight.fetch_sub(1, std::memory_order_acq_rel);
}
}

engine::PrismTransform piece_transform(double x0, double y0, double z0, double x1, double y1,
                                       double z1)
{
    const auto split = [](double value, float &local, std::int16_t &sector) {
        const double cell = engine::sector_size;
        long long index = 0;
        if (value > cell)
            index = static_cast<long long>(std::ceil((value - cell) / cell));
        else if (value < -cell)
            index = static_cast<long long>(std::floor((value + cell) / cell));
        local = static_cast<float>(value - static_cast<double>(index) * cell);
        sector = static_cast<std::int16_t>(std::clamp<long long>(index, INT16_MIN, INT16_MAX));
    };
    engine::PrismTransform t{};
    split((x0 + x1) * 0.5, t.x, t.sector_x);
    split((z0 + z1) * 0.5, t.z, t.sector_z);
    t.y = static_cast<float>((y0 + y1) * 0.5);

    // Line models extend along local +Z. Yaw turns +Z towards (dx, dz); the
    // pitch about local X raises the +Z end for a positive elevation.
    const double dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
    const double yaw = std::atan2(dx, dz);
    const double elevation = std::atan2(dy, std::hypot(dx, dz));
    const double cy = std::cos(yaw * 0.5), sy = std::sin(yaw * 0.5);
    const double cp = std::cos(elevation * 0.5), sp = std::sin(elevation * 0.5);
    t.qw = static_cast<float>(cy * cp);
    t.qx = static_cast<float>(-cy * sp);
    t.qy = static_cast<float>(sy * cp);
    t.qz = static_cast<float>(sy * sp);
    return t;
}

bool install(const engine::Functions &functions, std::string *error)
{
    fn = functions;
    if (MH_CreateHook(reinterpret_cast<void *>(fn.vehicle_render_dispatch),
                      reinterpret_cast<void *>(&hooked_dispatch),
                      reinterpret_cast<void **>(&original_dispatch)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void *>(fn.vehicle_render_dispatch)) != MH_OK)
    {
        MH_RemoveHook(reinterpret_cast<void *>(fn.vehicle_render_dispatch));
        if (error) *error = "could not hook the vehicle render pass";
        return false;
    }
    hook_installed = true;
    return true;
}

bool uninstall()
{
    if (!hook_installed) return true;
    MH_DisableHook(reinterpret_cast<void *>(fn.vehicle_render_dispatch));
    for (int i = 0; i < 200 && in_flight.load(std::memory_order_acquire) > 0; ++i) Sleep(5);
    hook_installed = false;
    return in_flight.load(std::memory_order_acquire) == 0;
}

void publish(std::vector<Piece> pieces)
{
    std::lock_guard lock(pieces_mutex);
    latest = std::move(pieces);
}

void release_models()
{
    suspended = true;
    {
        std::lock_guard lock(pieces_mutex);
        latest.clear();
    }
    if (!fn.model_transfer) return;
    std::lock_guard lock(models_mutex);
    std::size_t released = 0;
    try
    {
        for (auto &pool : pools)
            for (auto *&model : pool)
            {
                if (!model) continue;
                destroy_model(model);
                model = nullptr;
                ++released;
            }
    }
    catch (...)
    {
        faulted = true;
        log::error("Releasing guide-line models faulted; guide lines disabled for this session.");
    }
    if (released) log::info("Released " + std::to_string(released) + " guide-line models.");
    logged_first_render = false;
}

void resume()
{
    suspended = false;
}
}
