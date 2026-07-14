add_library(boot_pipeline STATIC
    include/boot_pipeline/iboot_detector.h
    include/boot_pipeline/detector_registry.h
    include/boot_pipeline/kernel_detector.h
    include/boot_pipeline/init_detector.h
    include/boot_pipeline/panic_detector.h
    include/boot_pipeline/boot_pipeline_manager.h
    src/detector_registry.cpp
    src/kernel_detector.cpp
    src/init_detector.cpp
    src/panic_detector.cpp
    src/boot_pipeline_manager.cpp
)

set_target_properties(boot_pipeline PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

target_include_directories(boot_pipeline PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(boot_pipeline PUBLIC
    core
    models
    boot_events
    boot_session
    Qt6::Core
)
