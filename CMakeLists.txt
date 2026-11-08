cmake_minimum_required(VERSION 3.16)
project(lab4 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(lab4 main.cpp Data.h)
target_link_libraries(lab4 PRIVATE Threads::Threads)