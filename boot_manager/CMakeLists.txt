add_library(boot_manager STATIC
    src/boot_manager.cpp
    src/boot_validator.cpp
)

target_include_directories(boot_manager PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(boot_manager PUBLIC
    core
    models
    image_parser
    image_converter
    Qt6::Core
    nlohmann_json::nlohmann_json
)
