add_library(image_parser STATIC
    src/bootimg_parser.cpp
    src/vbmeta_parser.cpp
    src/payload_parser.cpp
    src/super_parser.cpp
    src/ext4_parser.cpp
    src/image_parser.cpp
)

target_include_directories(image_parser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(image_parser PUBLIC
    core
    Qt6::Core
)
