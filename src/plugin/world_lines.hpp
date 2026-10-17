#pragma once

// In-world guide lines: plugin-owned Prism3D model instances submitted from
// the player's vehicle render pass, so they appear in every view and in the
// mirrors. Approach and engine calls follow the MIT-licensed "Reverse Posture
// Assistant For ATS" by yyysheng.

#include <string>
#include <vector>

#include "core/guides.hpp"
#include "plugin/engine.hpp"

namespace rto::world
{
struct Piece
{
    engine::PrismTransform transform;
    Band band = Band::green;
};

// Builds the transform for one line piece between two world points.
engine::PrismTransform piece_transform(double x0, double y0, double z0, double x1, double y1,
                                       double z1);

// MinHook must already be initialised. Hooks the vehicle render dispatch.
bool install(const engine::Functions &functions, std::string *error);
// Disables the hook and waits for in-flight calls; returns false if a thread
// may still be inside it (the caller must then leave MinHook initialised).
bool uninstall();

// Game thread: latest pieces to draw (empty hides everything).
void publish(std::vector<Piece> pieces);
// Game thread: destroy model instances before the scene is torn down
// (pause/menu/shutdown). Rendering resumes with fresh instances.
void release_models();
void resume();
}
