#pragma once

#include <filesystem>
#include <string>

namespace rto::log
{
// Must be called once from scs_telemetry_init. `game_log` is the SDK logger,
// which is only used from the game thread (init and telemetry callbacks).
void init(const std::filesystem::path &file, void (*game_log)(int type, const char *message));
void shutdown();

// Thread-safe file logging. `to_game` additionally forwards to the game
// console/log and must only be set from the game thread.
void info(const std::string &message, bool to_game = false);
void warn(const std::string &message, bool to_game = false);
void error(const std::string &message, bool to_game = false);
}
