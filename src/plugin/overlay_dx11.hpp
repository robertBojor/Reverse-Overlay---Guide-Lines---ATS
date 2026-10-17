#pragma once

#include <string>

#include "core/scene.hpp"
#include "plugin/config.hpp"

namespace rto::overlay
{
// Hooks IDXGISwapChain::Present/Present1/ResizeBuffers. Uses only DXGI/D3D11
// interfaces, never game code, so it is independent of the ATS build.
// MinHook must already be initialised.
bool install(std::string *error);
// Disables the hooks; returns false if a thread may still be inside one (the
// caller must then leave MinHook initialised).
bool uninstall();

// Called from the game thread; the render thread picks up the latest copy.
void publish(Scene scene);
void set_config(const Config &config);
}
