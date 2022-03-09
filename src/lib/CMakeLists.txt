set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(shiv    STATIC)
target_sources(shiv PRIVATE shiv.c)
target_include_directories(shiv
    PRIVATE "../include/shiv"
    INTERFACE "../include")
target_link_libraries(shiv PUBLIC Onyx::Onyx)
add_library(Shiv::Shiv ALIAS shiv)
#target_compile_definitions(shiv PUBLIC "COAL_SIMPLE_TYPE_NAMES")

if(WIN32)
set_target_properties(shiv PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()
