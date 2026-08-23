add_library(emulator_gdb STATIC
    register_snapshot.h
    register_snapshot.cpp
    rsp_decoder.h
    rsp_decoder.cpp
    breakpoint_manager.h
    breakpoint_manager.cpp
    instruction_tracer.h
    instruction_tracer.cpp
    gdb_session.h
    gdb_session.cpp
)


target_include_directories(emulator_gdb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)


target_link_libraries(emulator_gdb PUBLIC
    emulator_logger
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Windows>:ws2_32>
)
