add_library(qemu_backend STATIC
    src/qemu_detector.cpp
    src/qemu_arguments.cpp
    src/qemu_launcher.cpp
)

target_include_directories(qemu_backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(qemu_backend PUBLIC
    core
    models
    Qt6::Core
)
