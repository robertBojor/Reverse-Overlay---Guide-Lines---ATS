#include "plugin/log.hpp"

#include <windows.h>

#include <cstdio>
#include <fstream>
#include <mutex>

namespace rto::log
{
namespace
{
std::mutex mutex;
std::filesystem::path path;
void (*game_logger)(int, const char *) = nullptr;

void write(const char *level, int game_type, const std::string &message, bool to_game)
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "%02u:%02u:%02u.%03u", now.wHour, now.wMinute,
                  now.wSecond, now.wMilliseconds);
    {
        std::lock_guard lock(mutex);
        if (!path.empty())
        {
            std::ofstream out(path, std::ios::app);
            out << stamp << ' ' << level << ' ' << message << '\n';
        }
    }
    if (to_game && game_logger)
        game_logger(game_type, ("[reverse-overlay] " + message).c_str());
}
}

void init(const std::filesystem::path &file, void (*game_log)(int, const char *))
{
    std::lock_guard lock(mutex);
    path = file;
    game_logger = game_log;
    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    std::ofstream(path, std::ios::trunc) << "ATS Reverse Overlay log\n";
}

void shutdown()
{
    std::lock_guard lock(mutex);
    game_logger = nullptr;
}

// SCS_LOG_TYPE_message = 0, warning = 1, error = 2.
void info(const std::string &message, bool to_game) { write("INFO ", 0, message, to_game); }
void warn(const std::string &message, bool to_game) { write("WARN ", 1, message, to_game); }
void error(const std::string &message, bool to_game) { write("ERROR", 2, message, to_game); }
}
