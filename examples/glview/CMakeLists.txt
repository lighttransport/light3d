find_package(OpenGL REQUIRED)

add_executable(glview main.cpp)

target_link_libraries(glview PRIVATE
    light3d
    glad
    glfw
    OpenGL::GL
)
