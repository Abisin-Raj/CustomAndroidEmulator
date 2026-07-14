#include "core/logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace core {

namespace {

std::filesystem::path getExecutableDir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

} // namespace

void initLogger() {
    std::vector<spdlog::sink_ptr> sinks;
    
    // 1. Color console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    sinks.push_back(console_sink);

    // 2. File sink (logs/launcher.log relative to application executable directory)
    try {
        std::filesystem::path logDir = getExecutableDir() / "logs";
        std::filesystem::create_directories(logDir);
        
        std::filesystem::path logFile = logDir / "launcher.log";
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile.string(), true);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        sinks.push_back(file_sink);
    } catch (...) {
        // Fallback silently if logs directory is unwritable
    }

    auto logger = std::make_shared<spdlog::logger>("global", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    
    // Set default log level to debug
    spdlog::set_level(spdlog::level::debug);
    
    LOG_INFO("Core logging initialized. File output written to executable's logs/launcher.log.");
}

} // namespace core
