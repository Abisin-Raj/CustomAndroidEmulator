add_library(emulator_binder STATIC
    binder_transaction_record.h
    binder_transaction_record.cpp
    binder_ioctl_decoder.h
    binder_ioctl_decoder.cpp
    binder_stats.h
    binder_stats.cpp
    binder_telemetry.h
    binder_telemetry.cpp
)

target_include_directories(emulator_binder PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator_binder PUBLIC
    emulator_logger
    nlohmann_json::nlohmann_json
)
