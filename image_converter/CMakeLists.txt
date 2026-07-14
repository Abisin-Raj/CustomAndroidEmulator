cmake_minimum_required(VERSION 3.16)
project(image_converter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BROTLIDEC REQUIRED libbrotlidec)

add_library(image_converter STATIC
    src/brotli_decompressor.cpp
    src/transfer_list_parser.cpp
    src/dat_image_builder.cpp
)

target_include_directories(image_converter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${BROTLIDEC_INCLUDE_DIRS}
)

target_link_libraries(image_converter
    PRIVATE
        ${BROTLIDEC_LIBRARIES}
)

target_compile_features(image_converter PUBLIC cxx_std_17)
