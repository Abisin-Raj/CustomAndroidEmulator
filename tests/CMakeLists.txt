add_executable(execute_test_runner
    src/test_main.cpp
)

target_link_libraries(execute_test_runner PRIVATE
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
    spdlog::spdlog
    Qt6::Core
    Qt6::Network
)

add_custom_target(run_phase73_vm
    COMMAND execute_test_runner --run-real-vm
    DEPENDS execute_test_runner
    COMMENT "Executing Phase 7.3 Real Android ROM VM Boot Trace"
)


