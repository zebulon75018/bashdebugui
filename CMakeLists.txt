cmake_minimum_required(VERSION 3.10)
project(BashDbGui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(BashDbGui
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

target_link_libraries(BashDbGui PRIVATE Qt5::Widgets)
