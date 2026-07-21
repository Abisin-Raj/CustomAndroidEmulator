add_executable(run_tests
    src/test_main.cpp
)

target_link_libraries(run_tests PRIVATE
    core
    models
    image_parser
    image_converter
    boot_manager
    rom_manager
    boot_assets
    qemu_backend
    virtual_machine
    boot_events
    boot_session
    kernel_runtime
    boot_pipeline
    kernel_analysis
    firmware
    behavior_exploration_tool
)
