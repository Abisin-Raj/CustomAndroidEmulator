add_library(core STATIC
    src/logger.cpp
    src/utils.cpp
    src/config_manager.cpp
)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(core PUBLIC
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    emulator_logger
)

