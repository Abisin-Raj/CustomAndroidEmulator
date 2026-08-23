add_library(emulator_smc STATIC
    smc_call_record.h
    smc_call_record.cpp
    smc_retry_detector.h
    smc_retry_detector.cpp
    smc_telemetry.h
    smc_telemetry.cpp
)

target_include_directories(emulator_smc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator_smc PUBLIC
    emulator_logger
    nlohmann_json::nlohmann_json
)
