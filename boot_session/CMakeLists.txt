add_library(boot_session STATIC
    include/boot_session/boot_session.h
    src/boot_session.cpp
)

target_include_directories(boot_session PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(boot_session PUBLIC
    core
    models
    boot_events
)

