#pragma once

// Locates the Prism3D functions needed to place model instances in the world.
// Functions are found by wildcarded byte patterns (tools/re/make_patterns.py),
// never by fixed address, and each pattern must match exactly once.

#include <cstddef>
#include <cstdint>
#include <string>

namespace rto::engine
{
#pragma pack(push, 1)
// Prism3D placement: floating-origin local position (512 m cells) + quaternion.
struct PrismTransform
{
    float x;
    float y;
    float z;
    std::int16_t sector_x;
    std::int16_t sector_z;
    float qw;
    float qx;
    float qy;
    float qz;
};
#pragma pack(pop)
static_assert(sizeof(PrismTransform) == 32);

using ModelLoad = std::uint64_t *(__fastcall *)(std::uint64_t *, const char **,
                                                const std::uint64_t *, const std::uint64_t *, char);
using ModelTransfer = std::uint64_t *(__fastcall *)(std::uint64_t *, std::uint64_t *);
using ModelParameterInit = void(__fastcall *)(std::uint64_t *);
using SetTransform = void(__fastcall *)(std::uint64_t, const PrismTransform *);
using VehicleRenderDispatch = void(__fastcall *)(std::uint64_t, std::uint64_t);

struct Functions
{
    ModelLoad model_load = nullptr;
    ModelTransfer model_transfer = nullptr;
    ModelParameterInit model_parameter_init = nullptr;
    SetTransform set_transform = nullptr;
    VehicleRenderDispatch vehicle_render_dispatch = nullptr;
};

struct Resolution
{
    bool ok = false;
    bool verified_build = false; // executable hash is one tested in game
    std::string report;
};

// Scans the running executable. Fills `out` only when every pattern matches once.
Resolution resolve(Functions *out);

// Model instance layout facts used by the renderer (verified with the
// functions above on ATS 1.61.x; not covered by the patterns).
constexpr std::size_t placement_node_offset = 0x10;
constexpr std::size_t vtable_render_slot = 0x90 / sizeof(void *);
constexpr std::size_t vtable_finalize_slot = 0x140 / sizeof(void *);
constexpr float sector_size = 512.0f;
}
