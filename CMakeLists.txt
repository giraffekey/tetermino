cmake_minimum_required(VERSION 4.1)
project(tetermino C)

set(CMAKE_C_STANDARD 23)

add_executable(tetermino main.c)
target_link_libraries(tetermino m)
