cmake_minimum_required(VERSION 3.21)
project(QtPmuApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(QtPmuApp
    main.cpp
    mainwindow.cpp
    mainwindow.h
    pmusimulatorservice.cpp
    pmusimulatorservice.h
)

target_link_libraries(QtPmuApp PRIVATE Qt6::Widgets Qt6::Network)
