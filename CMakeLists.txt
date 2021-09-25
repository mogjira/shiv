cmake_minimum_required(VERSION 3.17)

project(Shiv VERSION 0.1.0)

option(SHIV_SKIP_EXAMPLES "Skip building examples" OFF)

if(NOT DEFINED OBSIDIAN_URL)
    set(OBSIDIAN_URL https://github.com/mokchira/obsidian)
endif()

include(FetchContent)

FetchContent_Declare(
    obsidian 
    GIT_REPOSITORY ${OBSIDIAN_URL}
    GIT_TAG newscene)

FetchContent_MakeAvailable(obsidian)

# So we can use the author_shaders function
list(APPEND CMAKE_MODULE_PATH ${obsidian_SOURCE_DIR}/cmake)

add_subdirectory(src/lib)
add_subdirectory(src/shaders)
if(NOT ${SHIV_SKIP_EXAMPLES})
    add_subdirectory(src/examples)
endif()
