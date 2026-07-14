add_executable(launcher
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/rom_card.cpp
    src/rom_card.h
)

target_link_libraries(launcher PRIVATE
    rom_manager
    Qt6::Widgets
    spdlog::spdlog
)

# On Windows, keep console window for debug builds
# set_target_properties(launcher PROPERTIES WIN32_EXECUTABLE TRUE)
# Let's keep console output visible during development for spdlog logging.
