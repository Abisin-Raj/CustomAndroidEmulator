add_library(emulator_milestones STATIC
    boot_milestone_tracker.h
    boot_milestone_tracker.cpp
    execution_watchdog.h
    execution_watchdog.cpp
)

target_include_directories(emulator_milestones PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator_milestones PUBLIC
    emulator_logger
    nlohmann_json::nlohmann_json
)
