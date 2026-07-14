add_library(firmware STATIC
    dispatcher/smc_id.cpp
    dispatcher/smc_dispatcher.cpp
    scm/scm_services.cpp
)

target_include_directories(firmware
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(firmware
    PUBLIC
        core
)

target_compile_features(firmware PRIVATE cxx_std_17)
