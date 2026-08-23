add_library(emulator_input STATIC
    input_event_record.h
    input_event_record.cpp
    virtual_touchscreen.h
    virtual_touchscreen.cpp
    input_telemetry.h
    input_telemetry.cpp
)

target_include_directories(emulator_input PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator_input PUBLIC
    emulator_logger
    nlohmann_json::nlohmann_json
)
