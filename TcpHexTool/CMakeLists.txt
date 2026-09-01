cmake_minimum_required(VERSION 3.5)

project(TcpHexTool)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    main.cpp
    MainWindow.cpp
    AltSystemParameters.cpp
)

set(HEADERS
    MainWindow.h
    AltSystemParameters.h
)

set(FORMS
    MainWindow.ui
)

add_executable(TcpHexTool
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
)

target_link_libraries(TcpHexTool
    Qt5::Widgets
    Qt5::Network
)
