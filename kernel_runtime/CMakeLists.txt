add_library(kernel_runtime STATIC
    include/kernel_runtime/kernel_commandline.h
    src/kernel_commandline.cpp
)

target_include_directories(kernel_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(kernel_runtime PUBLIC
    core
    models
)
