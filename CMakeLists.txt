cmake_minimum_required(VERSION 3.17)

project(Shiv VERSION 0.1.0)

option(SHIV_SKIP_EXAMPLES "Skip building examples" OFF)

if(UNIX)
set(git_base_url /home/michaelb/dev)
elseif(WIN32)
set(git_base_url C:/dev)
endif()

include(FetchContent)
FetchContent_Declare(
    cmake-common
    GIT_REPOSITORY ${git_base_url}/cmake-common
    GIT_TAG win)
FetchContent_GetProperties(cmake-common)
if(NOT ${cmake-common_POPULATED})
    FetchContent_Populate(cmake-common)
endif()
list(APPEND CMAKE_MODULE_PATH ${cmake-common_SOURCE_DIR})

include(header)

FetchContent_Declare(
    obsidian 
    GIT_REPOSITORY ${git_base_url}/obsidian 
    GIT_TAG win)

FetchContent_MakeAvailable(obsidian)

add_subdirectory(src/lib)
add_subdirectory(src/shaders)
if(NOT ${SHIV_SKIP_EXAMPLES})
    add_subdirectory(src/examples)
endif()
