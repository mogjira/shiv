add_executable(hello hello-shiv.c)
add_executable(scene scene.c)

if(WIN32)
set_target_properties(hello scene PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(hello Shiv::Shiv)
target_link_libraries(scene Shiv::Shiv)
