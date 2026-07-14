add_library(boot_events STATIC
    include/boot_events/event_types.h
    include/boot_events/event_dispatcher.h
    src/event_dispatcher.cpp
)

set_target_properties(boot_events PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

target_include_directories(boot_events PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(boot_events PUBLIC
    core
    models
    Qt6::Core
)
