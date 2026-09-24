#pragma once
#ifdef ERROR
#undef ERROR
#endif
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include "logger/api/unified_logger.h"

namespace core {
void initLogger();

inline ::emulator::logger::models::LogLevel getLogLevelInfo()  { return ::emulator::logger::models::LogLevel::INFO; }
inline ::emulator::logger::models::LogLevel getLogLevelWarn()  { return ::emulator::logger::models::LogLevel::WARN; }
inline ::emulator::logger::models::LogLevel getLogLevelError() { return ::emulator::logger::models::LogLevel::ERROR; }
inline ::emulator::logger::models::LogLevel getLogLevelDebug() { return ::emulator::logger::models::LogLevel::DEBUG; }
}

// ROM category loggers
#define LOG_ROM_INFO(...)       do { spdlog::info("[ROM] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelInfo(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_ROM_WARN(...)       do { spdlog::warn("[ROM] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelWarn(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_ROM_ERROR(...)      do { spdlog::error("[ROM] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelError(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_ROM_DEBUG(...)      do { spdlog::debug("[ROM] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelDebug(), fmt::format(__VA_ARGS__)); } while(0)

// Parser category loggers
#define LOG_PARSER_INFO(...)    do { spdlog::info("[Parser] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelInfo(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_PARSER_WARN(...)    do { spdlog::warn("[Parser] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelWarn(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_PARSER_ERROR(...)   do { spdlog::error("[Parser] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelError(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_PARSER_DEBUG(...)   do { spdlog::debug("[Parser] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::ROM, ::core::getLogLevelDebug(), fmt::format(__VA_ARGS__)); } while(0)

// Launcher category loggers
#define LOG_LAUNCHER_INFO(...)  do { spdlog::info("[Launcher] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::LAUNCHER, ::core::getLogLevelInfo(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_LAUNCHER_WARN(...)  do { spdlog::warn("[Launcher] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::LAUNCHER, ::core::getLogLevelWarn(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_LAUNCHER_ERROR(...) do { spdlog::error("[Launcher] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::LAUNCHER, ::core::getLogLevelError(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_LAUNCHER_DEBUG(...) do { spdlog::debug("[Launcher] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::LAUNCHER, ::core::getLogLevelDebug(), fmt::format(__VA_ARGS__)); } while(0)

// Boot category loggers
#define LOG_BOOT_INFO(...)      do { spdlog::info("[Boot] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::BOOT, ::core::getLogLevelInfo(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_BOOT_WARN(...)      do { spdlog::warn("[Boot] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::BOOT, ::core::getLogLevelWarn(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_BOOT_ERROR(...)     do { spdlog::error("[Boot] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::BOOT, ::core::getLogLevelError(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_BOOT_DEBUG(...)     do { spdlog::debug("[Boot] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::BOOT, ::core::getLogLevelDebug(), fmt::format(__VA_ARGS__)); } while(0)

// QEMU category loggers
#define LOG_QEMU_INFO(...)      do { spdlog::info("[QEMU] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::QEMU, ::core::getLogLevelInfo(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_QEMU_WARN(...)      do { spdlog::warn("[QEMU] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::QEMU, ::core::getLogLevelWarn(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_QEMU_ERROR(...)     do { spdlog::error("[QEMU] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::QEMU, ::core::getLogLevelError(), fmt::format(__VA_ARGS__)); } while(0)
#define LOG_QEMU_DEBUG(...)     do { spdlog::debug("[QEMU] " __VA_ARGS__); ::emulator::logger::api::UnifiedLogger::Instance().Log(::emulator::logger::models::Subsystem::QEMU, ::core::getLogLevelDebug(), fmt::format(__VA_ARGS__)); } while(0)

// Generic fallback logger
#ifndef LOG_GENERIC_INFO
#define LOG_GENERIC_INFO(...)  spdlog::info(__VA_ARGS__)
#define LOG_GENERIC_WARN(...)  spdlog::warn(__VA_ARGS__)
#define LOG_GENERIC_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_GENERIC_DEBUG(...) spdlog::debug(__VA_ARGS__)
#endif
