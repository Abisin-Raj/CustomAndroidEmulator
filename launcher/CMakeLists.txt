add_executable(launcher
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

target_link_libraries(launcher PRIVATE
    rom_manager
    Qt6::Widgets
    core
    image_parser
)
