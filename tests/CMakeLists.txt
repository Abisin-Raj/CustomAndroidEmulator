add_executable(run_tests
    src/test_main.cpp
)

target_link_libraries(run_tests PRIVATE
    core
    models
    image_parser
    boot_manager
    rom_manager
    boot_assets
    qemu_backend
    virtual_machine
)
