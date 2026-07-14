add_library(boot_assets STATIC
    src/boot_asset_loader.cpp
)

target_include_directories(boot_assets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(boot_assets PUBLIC
    core
    models
)
