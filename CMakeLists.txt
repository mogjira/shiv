cmake_minimum_required(VERSION 3.17)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/cmake-common)

project(Shiv VERSION 0.1.0)

include(header)

find_package(Hell)
find_package(Obsidian)
find_package(Coal)

add_subdirectory(cmake)
add_subdirectory(src/lib)
add_subdirectory(src/shaders)
add_subdirectory(src/examples)
