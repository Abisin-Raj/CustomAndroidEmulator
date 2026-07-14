add_library(kernel_analysis STATIC
    src/elf_loader.cpp
    src/symbol_resolver.cpp
)

target_include_directories(kernel_analysis PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(kernel_analysis PUBLIC
    core
)
