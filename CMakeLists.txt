cmake_minimum_required(VERSION 3.20)
project(CustomAndroidEmulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic compilation of Qt files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(LibArchive REQUIRED)

# Print package status
message(STATUS "Found Qt6: ${Qt6_VERSION}")
message(STATUS "Found LibArchive: ${LibArchive_VERSION}")

# Add subdirectories
add_subdirectory(core)
add_subdirectory(models)
add_subdirectory(image_parser)
add_subdirectory(boot_manager)
add_subdirectory(boot_assets)
add_subdirectory(boot_events)
add_subdirectory(boot_session)
add_subdirectory(kernel_runtime)
add_subdirectory(qemu_backend)
add_subdirectory(boot_pipeline)
add_subdirectory(virtual_machine)
add_subdirectory(rom_manager)
add_subdirectory(launcher)
add_subdirectory(tests)
