cmake_minimum_required(VERSION 3.24)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" RTO_VERSION LIMIT_COUNT 1)
project(ats_reverse_overlay VERSION ${RTO_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: the plugin must load without a VC++ redistributable.
# CMake adds /EHsc by default; exception models are set per target below.
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Our own code: strict warnings. Third-party code is compiled as-is.
function(rto_strict target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# --- core tests (platform neutral) -----------------------------------------
add_executable(core_tests tests/core_tests.cpp)
add_executable(scene_preview tests/scene_preview.cpp)
rto_strict(core_tests)
rto_strict(scene_preview)
if(MSVC)
    target_compile_options(core_tests PRIVATE /EHsc)
    target_compile_options(scene_preview PRIVATE /EHsc)
endif()

enable_testing()
add_test(NAME core_tests COMMAND core_tests)

# --- plugin ------------------------------------------------------------------
option(RTO_BUILD_PLUGIN "Build the telemetry plugin DLL" ON)
if(WIN32 AND RTO_BUILD_PLUGIN)
    add_library(minhook STATIC
        third_party/minhook/src/buffer.c
        third_party/minhook/src/hook.c
        third_party/minhook/src/trampoline.c
        third_party/minhook/src/hde/hde64.c)
    target_include_directories(minhook PUBLIC third_party/minhook/include)
    if(MSVC)
        target_compile_options(minhook PRIVATE /W0)
    endif()

    add_library(imgui STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/imgui_impl_dx11.cpp)
    target_include_directories(imgui PUBLIC third_party/imgui)
    target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS)
    if(MSVC)
        target_compile_options(imgui PRIVATE /W0 /EHsc)
    endif()

    add_library(ats_reverse_overlay SHARED
        src/plugin/plugin.cpp
        src/plugin/overlay_dx11.cpp
        src/plugin/config.cpp
        src/plugin/engine.cpp
        src/plugin/log.cpp
        src/plugin/world_lines.cpp)
    target_include_directories(ats_reverse_overlay PRIVATE
        src
        third_party/scs_sdk/include)
    target_link_libraries(ats_reverse_overlay PRIVATE minhook imgui d3d11 dxgi shell32 ole32 bcrypt)
    rto_strict(ats_reverse_overlay)
    target_compile_definitions(ats_reverse_overlay PRIVATE RTO_VERSION="${RTO_VERSION}")
    if(MSVC)
        # /EHa: catch(...) in the render hook also catches access violations
        # while still running destructors (mutex unlocks).
        target_compile_options(ats_reverse_overlay PRIVATE /EHa)
        target_link_options(ats_reverse_overlay PRIVATE "/DEF:${CMAKE_SOURCE_DIR}/src/plugin/exports.def")
    endif()
    set_target_properties(ats_reverse_overlay PROPERTIES OUTPUT_NAME "ats_reverse_overlay")
endif()
