#include "core/logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace core {

void initLogger() {
    // Setup colorized stdout sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    // Pattern: [Timestamp] [Level] [Thread] message
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    
    auto logger = std::make_shared<spdlog::logger>("global", console_sink);
    spdlog::set_default_logger(logger);
    
    // Set level (can be configured later from settings)
    spdlog::set_level(spdlog::level::debug);
    
    LOG_INFO("Core logging initialized.");
}

} // namespace core
