cmake_minimum_required(VERSION 3.17)

project(Shiv VERSION 0.1.0)

option(SHIV_SKIP_EXAMPLES "Skip building examples" OFF)

if(NOT DEFINED ONYX_URL)
    set(ONYX_URL https://github.com/mokchira/onyx)
endif()

include(FetchContent)

FetchContent_Declare(
    onyx
    GIT_REPOSITORY ${ONYX_URL}
    GIT_TAG master)

FetchContent_MakeAvailable(onyx)

# So we can use the author_shaders function
list(APPEND CMAKE_MODULE_PATH ${onyx_SOURCE_DIR}/cmake)

add_subdirectory(src/lib)
add_subdirectory(src/shaders)
if(NOT ${SHIV_SKIP_EXAMPLES})
    add_subdirectory(src/examples)
endif()
