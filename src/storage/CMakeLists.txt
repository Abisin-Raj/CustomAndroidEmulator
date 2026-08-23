add_library(emulator_storage STATIC
    scsi_command_record.h
    scsi_command_record.cpp
    ufs_telemetry.h
    ufs_telemetry.cpp
)

target_include_directories(emulator_storage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator_storage PUBLIC
    emulator_logger
    nlohmann_json::nlohmann_json
)
