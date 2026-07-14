add_library(qemu_backend STATIC
    include/qemu_backend/qemu_detector.h
    include/qemu_backend/qemu_arguments.h
    include/qemu_backend/qemu_launcher.h
    src/qemu_detector.cpp
    src/qemu_arguments.cpp
    src/qemu_launcher.cpp
)

set_target_properties(qemu_backend PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
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
