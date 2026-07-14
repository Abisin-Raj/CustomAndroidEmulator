add_library(virtual_machine STATIC
    include/virtual_machine/vm_profile.h
    include/virtual_machine/vm_configuration_builder.h
    include/virtual_machine/vm_manager.h
    src/vm_profile.cpp
    src/vm_configuration_builder.cpp
    src/vm_manager.cpp
)

set_target_properties(virtual_machine PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
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
    boot_events
    boot_session
    kernel_runtime
    boot_pipeline
    Qt6::Core
    nlohmann_json::nlohmann_json
)
