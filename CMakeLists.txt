cmake_minimum_required(VERSION 3.17)

project(Shiv VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(
    cmake-common
    GIT_REPOSITORY https://github.com/mokchira/cmake-common
    GIT_TAG origin/master)
FetchContent_Populate(cmake-common)
list(APPEND CMAKE_MODULE_PATH ${cmake-common_SOURCE_DIR})

include(header)

find_package(Hell)
find_package(Obsidian)
find_package(Coal)

add_subdirectory(cmake)
add_subdirectory(src/lib)
add_subdirectory(src/shaders)
add_subdirectory(src/examples)
