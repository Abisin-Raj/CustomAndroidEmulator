cmake_minimum_required(VERSION 3.20)
project(behavior_exploration_tool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

# Find dependencies
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# Headers and Sources
set(HEADERS
    include/behavior_exploration_tool/bet_types.h
    include/behavior_exploration_tool/event_recorder.h
    include/behavior_exploration_tool/boot_timeline.h
    include/behavior_exploration_tool/progress_analyzer.h
    include/behavior_exploration_tool/comparison_engine.h
    include/behavior_exploration_tool/blocker_detector.h
    include/behavior_exploration_tool/report_generator.h
    include/behavior_exploration_tool/experiment_database.h
    include/behavior_exploration_tool/bet_system.h
    include/behavior_exploration_tool/override_engine.h
    include/behavior_exploration_tool/bet_replayer.h
)

set(SOURCES
    src/event_recorder.cpp
    src/boot_timeline.cpp
    src/progress_analyzer.cpp
    src/comparison_engine.cpp
    src/blocker_detector.cpp
    src/report_generator.cpp
    src/experiment_database.cpp
    src/bet_system.cpp
    src/override_engine.cpp
    src/bet_replayer.cpp
)

add_library(behavior_exploration_tool STATIC
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(behavior_exploration_tool PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(behavior_exploration_tool PUBLIC
    Qt6::Core
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
