add_executable(hello hello-shiv.c)
add_executable(scene scene.c)

target_link_libraries(hello Shiv::Shiv)
target_link_libraries(scene Shiv::Shiv)
