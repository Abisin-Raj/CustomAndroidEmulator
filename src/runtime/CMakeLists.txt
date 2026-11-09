add_library(emulator_runtime STATIC
    pixel_source.h
    sha256_util.h
    synthetic_pattern_pixel_source.h
    synthetic_pattern_pixel_source.cpp
    ramfb_bootanim_pixel_source.h
    ramfb_bootanim_pixel_source.cpp
    display_pipeline.h
    display_pipeline.cpp
    framebuffer_surface.h
    framebuffer_surface.cpp
    mouse_input_translator.h
    mouse_input_translator.cpp
    keyboard_translator.h
    keyboard_translator.cpp
    clipboard_bridge.h
    clipboard_bridge.cpp
    display_window.h
    display_window.cpp
)

target_include_directories(emulator_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(emulator_runtime PUBLIC
    emulator_input
    emulator_logger
    emulator_gdb
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(emulator_runtime PUBLIC gdi32 user32)
endif()
