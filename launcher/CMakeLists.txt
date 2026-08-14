add_executable(launcher
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/display_viewport.cpp
    src/display_viewport.h
    src/hardware_button_widget.cpp
    src/hardware_button_widget.h
    include/hardware_keys.h
)

target_include_directories(launcher PRIVATE
    include
    src
)

target_link_libraries(launcher PRIVATE
    rom_manager
    Qt6::Widgets
    core
    models
    image_parser
    boot_manager
    boot_assets
    qemu_backend
    virtual_machine
    boot_events
    boot_session
    kernel_runtime
    boot_pipeline
    behavior_exploration_tool
)
