add_library(virtual_machine STATIC
    src/vm_profile.cpp
    src/vm_configuration_builder.cpp
    src/vm_manager.cpp
)

target_include_directories(virtual_machine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(virtual_machine PUBLIC
    core
    models
    boot_assets
    qemu_backend
    Qt6::Core
    nlohmann_json::nlohmann_json
)
