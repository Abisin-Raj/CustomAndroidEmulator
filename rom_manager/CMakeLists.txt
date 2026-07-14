add_library(rom_manager STATIC
    src/rom_manager.cpp
)

target_include_directories(rom_manager PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(rom_manager PUBLIC
    nlohmann_json::nlohmann_json
    LibArchive::LibArchive
    Qt6::Core
    core
    image_parser
)
