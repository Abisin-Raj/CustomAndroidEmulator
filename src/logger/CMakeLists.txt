add_library(emulator_logger STATIC
    core/log_ring_buffer.h
    core/log_ring_buffer.cpp
    core/logger_core.h
    core/logger_core.cpp
    sinks/log_sink.h
    sinks/file_sink.h
    sinks/file_sink.cpp
    sinks/timeline_sink.h
    sinks/timeline_sink.cpp
    analyzers/diagnostics_analyzer.h
    analyzers/diagnostics_analyzer.cpp
    session/session_manager.h
    session/session_manager.cpp
    api/unified_logger.h
    api/unified_logger.cpp
    models/log_level.h
    models/subsystem.h
    models/log_entry.h
    models/session_manifest.h
    models/metrics.h
)

target_include_directories(emulator_logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)


target_link_libraries(emulator_logger PUBLIC
    nlohmann_json::nlohmann_json
)
