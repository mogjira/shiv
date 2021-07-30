set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(shivObj OBJECT)
add_library(shiv    SHARED)
target_sources(shivObj PRIVATE shiv.c)
target_include_directories(shivObj 
    PRIVATE "../include/shiv"
    INTERFACE "../include")
target_link_libraries(shiv PUBLIC shivObj Obsidian::Obsidian)
add_library(Shiv::ShivObj ALIAS shivObj)
add_library(Shiv::Shiv    ALIAS shiv)
