add_executable(launcher
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
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
)
