#pragma once
#include <spdlog/spdlog.h>

// ROM category loggers
#define LOG_ROM_INFO(...)       spdlog::info("[ROM] " __VA_ARGS__)
#define LOG_ROM_WARN(...)       spdlog::warn("[ROM] " __VA_ARGS__)
#define LOG_ROM_ERROR(...)      spdlog::error("[ROM] " __VA_ARGS__)
#define LOG_ROM_DEBUG(...)      spdlog::debug("[ROM] " __VA_ARGS__)

// Parser category loggers
#define LOG_PARSER_INFO(...)    spdlog::info("[Parser] " __VA_ARGS__)
#define LOG_PARSER_WARN(...)    spdlog::warn("[Parser] " __VA_ARGS__)
#define LOG_PARSER_ERROR(...)   spdlog::error("[Parser] " __VA_ARGS__)
#define LOG_PARSER_DEBUG(...)   spdlog::debug("[Parser] " __VA_ARGS__)

// Launcher category loggers
#define LOG_LAUNCHER_INFO(...)  spdlog::info("[Launcher] " __VA_ARGS__)
#define LOG_LAUNCHER_WARN(...)  spdlog::warn("[Launcher] " __VA_ARGS__)
#define LOG_LAUNCHER_ERROR(...) spdlog::error("[Launcher] " __VA_ARGS__)
#define LOG_LAUNCHER_DEBUG(...) spdlog::debug("[Launcher] " __VA_ARGS__)

// Boot category loggers
#define LOG_BOOT_INFO(...)      spdlog::info("[Boot] " __VA_ARGS__)
#define LOG_BOOT_WARN(...)      spdlog::warn("[Boot] " __VA_ARGS__)
#define LOG_BOOT_ERROR(...)     spdlog::error("[Boot] " __VA_ARGS__)
#define LOG_BOOT_DEBUG(...)     spdlog::debug("[Boot] " __VA_ARGS__)

// Generic/Legacy fallback loggers
#define LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)

namespace core {
void initLogger();
}
