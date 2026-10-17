#include "plugin/overlay_dx11.hpp"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <vector>

#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>

#include "core/panel_layout.hpp"
#include "plugin/log.hpp"

namespace rto::overlay
{
namespace
{
using PresentFn = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain1 *, UINT, UINT,
                                                 const DXGI_PRESENT_PARAMETERS *);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT, UINT,
                                                      DXGI_FORMAT, UINT);

void *present_target = nullptr;
void *present1_target = nullptr;
void *resize_target = nullptr;
PresentFn original_present = nullptr;
Present1Fn original_present1 = nullptr;
ResizeBuffersFn original_resize = nullptr;
bool hooks_installed = false;

// Number of threads currently inside one of our hook functions. Uninstall
// waits for this to reach zero before MinHook frees the trampolines.
std::atomic<int> in_flight{0};
std::atomic<bool> shutting_down{false};
thread_local int present_depth = 0;

// Shared with the game thread.
std::mutex scene_mutex;
Scene latest_scene;
Config latest_config;

// Render-thread state, guarded by render_mutex.
std::mutex render_mutex;
bool disabled = false; // unsupported renderer or failure: stay passive
bool imgui_ready = false;
ID3D11Device *device = nullptr;
ID3D11DeviceContext *context = nullptr;
IDXGISwapChain *bound_swapchain = nullptr;
ID3D11RenderTargetView *rtv = nullptr;
HWND game_window = nullptr;
float font_built_for_height = 0.0f;
ImFont *font_regular = nullptr;
ImFont *font_bold = nullptr;
bool hotkey_was_down = false;
bool toggled_off = false;
float fade = 0.0f;
bool have_view = false;
bool logged_first_draw = false;
bool logged_device_objects_failure = false;
bool last_want = false;
PanelView smoothed_view;
std::chrono::steady_clock::time_point last_frame;

struct InFlight
{
    InFlight() { in_flight.fetch_add(1, std::memory_order_acq_rel); }
    ~InFlight() { in_flight.fetch_sub(1, std::memory_order_acq_rel); }
};

void release_rtv()
{
    if (rtv)
    {
        rtv->Release();
        rtv = nullptr;
    }
}

void teardown_render_state()
{
    release_rtv();
    if (imgui_ready)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        imgui_ready = false;
    }
    if (context)
    {
        context->Release();
        context = nullptr;
    }
    if (device)
    {
        device->Release();
        device = nullptr;
    }
    bound_swapchain = nullptr;
    font_regular = font_bold = nullptr;
    font_built_for_height = 0.0f;
    have_view = false;
}

void build_fonts(float screen_height)
{
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    const float size = std::max(13.0f, 17.0f * screen_height / 1080.0f);
    char windows_dir[MAX_PATH] = "C:\\Windows";
    GetWindowsDirectoryA(windows_dir, MAX_PATH);
    const std::filesystem::path fonts = std::filesystem::path(windows_dir) / "Fonts";
    ImFontConfig config;
    config.OversampleH = 2;
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0};
    const auto load = [&](const char *file) -> ImFont * {
        const auto path = (fonts / file).string();
        if (!std::filesystem::exists(path)) return nullptr;
        return io.Fonts->AddFontFromFileTTF(path.c_str(), size, &config, ranges);
    };
    font_regular = load("segoeui.ttf");
    font_bold = load("segoeuib.ttf");
    if (!font_regular)
    {
        ImFontConfig fallback;
        fallback.SizePixels = size;
        font_regular = io.Fonts->AddFontDefault(&fallback);
    }
    if (!font_bold) font_bold = font_regular;
    font_built_for_height = screen_height;
}

bool ensure_initialized(IDXGISwapChain *swapchain)
{
    if (imgui_ready && swapchain == bound_swapchain) return true;

    ID3D11Device *candidate = nullptr;
    if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device),
                                    reinterpret_cast<void **>(&candidate))) ||
        !candidate)
    {
        // DX12/Vulkan-backed swapchain: not supported, stay out of the way.
        if (!imgui_ready)
        {
            disabled = true;
            log::warn("Swap chain is not Direct3D 11; overlay disabled. Select DirectX 11 in "
                      "the game's graphics settings.");
        }
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    swapchain->GetDesc(&desc);
    if (!desc.OutputWindow || !IsWindowVisible(desc.OutputWindow))
    {
        candidate->Release();
        return false;
    }

    if (imgui_ready && candidate == device)
    {
        // Same device, new swap chain (e.g. after a display-mode change).
        candidate->Release();
        release_rtv();
        bound_swapchain = swapchain;
        game_window = desc.OutputWindow;
        log::info("Rebound to a new swap chain on the same device.");
        return true;
    }

    teardown_render_state();
    device = candidate;
    device->GetImmediateContext(&context);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    if (!ImGui_ImplDX11_Init(device, context))
    {
        log::error("ImGui DX11 backend failed to initialise; overlay disabled.");
        ImGui::DestroyContext();
        disabled = true;
        return false;
    }
    imgui_ready = true;
    bound_swapchain = swapchain;
    game_window = desc.OutputWindow;
    last_frame = std::chrono::steady_clock::now();
    char message[160];
    std::snprintf(message, sizeof(message),
                  "Overlay attached to D3D11 swap chain %ux%u format %d.",
                  desc.BufferDesc.Width, desc.BufferDesc.Height,
                  static_cast<int>(desc.BufferDesc.Format));
    log::info(message);
    return true;
}

bool ensure_rtv(IDXGISwapChain *swapchain)
{
    if (rtv) return true;
    ID3D11Texture2D *back_buffer = nullptr;
    if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void **>(&back_buffer))))
        return false;
    D3D11_TEXTURE2D_DESC tex{};
    back_buffer->GetDesc(&tex);
    // ImGui colours are authored for a non-sRGB target. If the buffer is an
    // sRGB format, view it as UNORM so overlay colours are not gamma-shifted.
    D3D11_RENDER_TARGET_VIEW_DESC view{};
    view.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    view.Format = tex.Format;
    if (tex.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) view.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    if (tex.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    HRESULT hr = device->CreateRenderTargetView(back_buffer, &view, &rtv);
    if (FAILED(hr)) hr = device->CreateRenderTargetView(back_buffer, nullptr, &rtv);
    back_buffer->Release();
    return SUCCEEDED(hr);
}

ImU32 to_imgui(Color c, float alpha_scale)
{
    const int a = static_cast<int>(c.a * std::clamp(alpha_scale, 0.0f, 1.0f) + 0.5f);
    return IM_COL32(c.r, c.g, c.b, a);
}

Rect panel_rect(const Config &config, float width, float height)
{
    const float h = config.panel_height * height;
    const float w = h * config.aspect;
    const float m = config.margin * height;
    float x = width - w - m, y = (height - h) * 0.5f;
    switch (config.anchor)
    {
    case Anchor::left: x = m; break;
    case Anchor::right: break;
    case Anchor::top_left: x = m; y = m; break;
    case Anchor::top_right: y = m; break;
    case Anchor::bottom_left: x = m; y = height - h - m; break;
    case Anchor::bottom_right: y = height - h - m; break;
    }
    x += config.offset_x * height;
    y += config.offset_y * height;
    return {x, y, w, h};
}

void poll_hotkey(const Config &config)
{
    if (config.toggle_vk <= 0 || GetForegroundWindow() != game_window)
    {
        hotkey_was_down = false;
        return;
    }
    const auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    const bool pressed = down(config.toggle_vk) &&
                         down(VK_CONTROL) == config.toggle_ctrl &&
                         down(VK_SHIFT) == config.toggle_shift &&
                         down(VK_MENU) == config.toggle_alt;
    if (pressed && !hotkey_was_down)
    {
        toggled_off = !toggled_off;
        log::info(toggled_off ? "Overlay hidden by hotkey." : "Overlay shown by hotkey.");
    }
    hotkey_was_down = pressed;
}

void draw_panel(ImDrawList *dl, const Scene &scene, const Config &config, float width,
                float height, float alpha)
{
    const float ui = height / 1080.0f;
    const Rect panel = panel_rect(config, width, height);
    const float opacity = config.opacity * alpha;
    const ImVec2 p0{panel.x, panel.y}, p1{panel.x + panel.w, panel.y + panel.h};

    dl->AddRectFilled(p0, p1, IM_COL32(14, 16, 20, static_cast<int>(200 * opacity)), 10.0f * ui);
    const Color border = scene.warning ? palette::danger : Color{255, 255, 255, 48};
    dl->AddRect(p0, p1, to_imgui(border, opacity), 10.0f * ui, 0, 1.5f * ui);

    PanelView target = fit_panel(scene, panel, ui);
    smoothed_view = have_view ? smooth_view(smoothed_view, target, 0.18f) : target;
    smoothed_view.screen_centre = target.screen_centre; // panel may have moved
    have_view = true;
    const PanelView &view = smoothed_view;

    const Rect area = drawing_area(panel, ui);
    dl->PushClipRect({area.x, area.y}, {area.x + area.w, area.y + area.h}, true);
    for (const auto &line : grid_lines(view, panel, ui))
        dl->AddLine({line.a.x, line.a.y}, {line.b.x, line.b.y}, to_imgui(palette::grid, opacity),
                    1.0f * ui);

    // Fills are drawn without anti-aliasing so adjacent sweep quads do not
    // show seams; outlines keep it.
    const ImDrawListFlags saved_flags = dl->Flags;
    std::vector<ImVec2> points;
    for (const auto &shape : scene.shapes)
    {
        points.clear();
        for (const auto &p : shape.points)
        {
            const auto s = view.to_screen(p);
            points.push_back({s.x, s.y});
        }
        if (points.size() < 2) continue;
        if (shape.kind == Shape::Kind::convex_fill)
        {
            dl->Flags = saved_flags & ~ImDrawListFlags_AntiAliasedFill;
            dl->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()),
                                    to_imgui(shape.color, opacity));
            dl->Flags = saved_flags;
        }
        else
        {
            dl->AddPolyline(points.data(), static_cast<int>(points.size()),
                            to_imgui(shape.color, opacity),
                            shape.closed ? ImDrawFlags_Closed : ImDrawFlags_None,
                            shape.thickness * ui);
        }
    }
    const float label_size = font_regular ? font_regular->FontSize * 0.8f : 13.0f * ui;
    for (const auto &label : scene.labels)
    {
        const auto s = view.to_screen(label.at);
        dl->AddText(font_regular, label_size, {s.x, s.y - label_size * 0.5f}, to_imgui(label.color, opacity),
                    label.text.c_str());
    }
    dl->PopClipRect();

    // Header and status lines.
    const float pad = 14.0f * ui;
    const float size = font_regular ? font_regular->FontSize : 17.0f * ui;
    float y = panel.y + pad * 0.7f;
    dl->AddText(font_bold, size, {panel.x + pad, y}, to_imgui(palette::text, opacity), "REVERSE");
    for (const auto &line : scene.status)
    {
        y += size * 1.1f;
        const bool warning = line.rfind("JACKKNIFE", 0) == 0;
        dl->AddText(warning ? font_bold : font_regular, size, {panel.x + pad, y},
                    to_imgui(warning ? palette::danger : palette::text, opacity), line.c_str());
    }
}

void render(IDXGISwapChain *swapchain)
{
    std::lock_guard lock(render_mutex);
    if (disabled || shutting_down.load(std::memory_order_acquire)) return;
    if (bound_swapchain && swapchain != bound_swapchain && imgui_ready)
    {
        // A second swap chain (or a replacement) is presenting. Only switch
        // if it belongs to a visible window; otherwise keep the current one.
        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(swapchain->GetDesc(&desc)) || desc.OutputWindow != game_window) return;
    }
    if (!ensure_initialized(swapchain)) return;

    Scene scene;
    Config config;
    {
        std::lock_guard scene_lock(scene_mutex);
        scene = latest_scene;
        config = latest_config;
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - last_frame).count(), 0.0f, 0.25f);
    last_frame = now;

    poll_hotkey(config);
    const bool want = config.enabled && !toggled_off && scene.visible;
    if (want != last_want && scene.visible)
    {
        log::info(want ? "Panel showing."
                       : (toggled_off ? "Panel suppressed: hidden by hotkey."
                                      : "Panel suppressed: disabled in settings."));
    }
    last_want = want;
    fade = std::clamp(fade + (want ? dt : -dt) / 0.2f, 0.0f, 1.0f);
    if (fade <= 0.0f)
    {
        have_view = false;
        return;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapchain->GetDesc(&desc))) return;
    const float width = static_cast<float>(desc.BufferDesc.Width);
    const float height = static_cast<float>(desc.BufferDesc.Height);
    if (width < 64.0f || height < 64.0f) return;
    if (!ensure_rtv(swapchain)) return;

    if (std::abs(font_built_for_height - height) > 1.0f)
    {
        build_fonts(height);
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = {width, height};
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;
    ImGui_ImplDX11_NewFrame();
    if (!io.Fonts->TexID)
    {
        // Shader compilation (d3dcompiler_47.dll) or texture creation failed.
        if (!logged_device_objects_failure)
        {
            logged_device_objects_failure = true;
            log::error("ImGui could not create its D3D11 objects; nothing can be drawn.");
        }
        return;
    }
    ImGui::NewFrame();
    draw_panel(ImGui::GetBackgroundDrawList(), scene, config, width, height, fade);
    ImGui::Render();
    if (!logged_first_draw)
    {
        logged_first_draw = true;
        const Rect r = panel_rect(config, width, height);
        char message[200];
        std::snprintf(message, sizeof(message),
                      "First panel draw: rect=(%.0f, %.0f, %.0f x %.0f) shapes=%zu vertices=%d",
                      r.x, r.y, r.w, r.h, scene.shapes.size(), ImGui::GetDrawData()->TotalVtxCount);
        log::info(message);
    }

    // Preserve the game's output-merger binding around our draw.
    ID3D11RenderTargetView *saved_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView *saved_dsv = nullptr;
    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved_rtvs, &saved_dsv);
    context->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // backs up/restores pipeline state
    context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, saved_rtvs, saved_dsv);
    for (auto *view : saved_rtvs)
        if (view) view->Release();
    if (saved_dsv) saved_dsv->Release();
}

// The plugin is built with /EHa, so catch(...) also catches structured
// exceptions (access violations) and still unwinds the render mutex.
void render_safely(IDXGISwapChain *swapchain)
{
    if (disabled) return;
    try
    {
        render(swapchain);
    }
    catch (...)
    {
        disabled = true; // never take the game down with us
        log::error("Overlay rendering raised an exception; overlay disabled for this session.");
    }
}

HRESULT STDMETHODCALLTYPE hooked_present(IDXGISwapChain *swapchain, UINT sync, UINT flags)
{
    InFlight guard;
    // Present1 may be implemented on top of Present (or vice versa); draw once.
    if (present_depth++ == 0 && !(flags & DXGI_PRESENT_TEST)) render_safely(swapchain);
    const HRESULT hr = original_present(swapchain, sync, flags);
    --present_depth;
    return hr;
}

HRESULT STDMETHODCALLTYPE hooked_present1(IDXGISwapChain1 *swapchain, UINT sync, UINT flags,
                                          const DXGI_PRESENT_PARAMETERS *params)
{
    InFlight guard;
    if (present_depth++ == 0 && !(flags & DXGI_PRESENT_TEST)) render_safely(swapchain);
    const HRESULT hr = original_present1(swapchain, sync, flags, params);
    --present_depth;
    return hr;
}

HRESULT STDMETHODCALLTYPE hooked_resize(IDXGISwapChain *swapchain, UINT count, UINT w, UINT h,
                                        DXGI_FORMAT format, UINT flags)
{
    InFlight guard;
    {
        // Our back-buffer view must be released or ResizeBuffers fails.
        std::lock_guard lock(render_mutex);
        if (swapchain == bound_swapchain) release_rtv();
    }
    return original_resize(swapchain, count, w, h, format, flags);
}

bool find_swapchain_vtable(std::string *error)
{
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ats_reverse_overlay_probe";
    RegisterClassExW(&wc);
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
                                  nullptr, nullptr, wc.hInstance, nullptr);
    if (!window)
    {
        *error = "could not create probe window";
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1;
    desc.BufferDesc.Width = 64;
    desc.BufferDesc.Height = 64;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain *swapchain = nullptr;
    ID3D11Device *probe_device = nullptr;
    ID3D11DeviceContext *probe_context = nullptr;
    HRESULT hr = E_FAIL;
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, nullptr, 0,
                                           D3D11_SDK_VERSION, &desc, &swapchain, &probe_device,
                                           nullptr, &probe_context);
        if (SUCCEEDED(hr)) break;
    }
    if (SUCCEEDED(hr))
    {
        void **vtable = *reinterpret_cast<void ***>(swapchain);
        present_target = vtable[8];
        resize_target = vtable[13];
        IDXGISwapChain1 *swapchain1 = nullptr;
        if (SUCCEEDED(swapchain->QueryInterface(__uuidof(IDXGISwapChain1),
                                                reinterpret_cast<void **>(&swapchain1))))
        {
            present1_target = (*reinterpret_cast<void ***>(swapchain1))[22];
            swapchain1->Release();
        }
        swapchain->Release();
        probe_context->Release();
        probe_device->Release();
    }
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (FAILED(hr))
    {
        char message[96];
        std::snprintf(message, sizeof(message), "probe D3D11 device failed (0x%08lx)",
                      static_cast<unsigned long>(hr));
        *error = message;
        return false;
    }
    return true;
}
}

bool install(std::string *error)
{
    std::string reason;
    if (!find_swapchain_vtable(&reason))
    {
        if (error) *error = reason;
        return false;
    }
    std::vector<void *> targets{present_target, resize_target};
    bool ok = MH_CreateHook(present_target, reinterpret_cast<void *>(&hooked_present),
                            reinterpret_cast<void **>(&original_present)) == MH_OK &&
              MH_CreateHook(resize_target, reinterpret_cast<void *>(&hooked_resize),
                            reinterpret_cast<void **>(&original_resize)) == MH_OK;
    if (ok && present1_target)
    {
        targets.push_back(present1_target);
        ok = MH_CreateHook(present1_target, reinterpret_cast<void *>(&hooked_present1),
                           reinterpret_cast<void **>(&original_present1)) == MH_OK;
    }
    for (void *target : targets)
        ok = ok && MH_EnableHook(target) == MH_OK;
    if (!ok)
    {
        for (void *target : targets) MH_RemoveHook(target);
        if (error) *error = "could not hook IDXGISwapChain";
        return false;
    }
    hooks_installed = true;
    return true;
}

bool uninstall()
{
    if (!hooks_installed) return true;
    shutting_down.store(true, std::memory_order_release);
    MH_DisableHook(present_target);
    MH_DisableHook(resize_target);
    if (present1_target) MH_DisableHook(present1_target);
    // Let any thread still inside a hook leave before trampolines are freed.
    for (int i = 0; i < 200 && in_flight.load(std::memory_order_acquire) > 0; ++i) Sleep(5);
    {
        std::lock_guard lock(render_mutex);
        teardown_render_state();
    }
    hooks_installed = false;
    return in_flight.load(std::memory_order_acquire) == 0;
}

void publish(Scene scene)
{
    std::lock_guard lock(scene_mutex);
    latest_scene = std::move(scene);
}

void set_config(const Config &config)
{
    std::lock_guard lock(scene_mutex);
    latest_config = config;
}
}
