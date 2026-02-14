set(GLVIEW_SOURCES main.cpp)
set(GLVIEW_LIBS light3d glfw)
set(GLVIEW_DEFS "")

# OpenGL support
if(LIGHT3D_ENABLE_OPENGL)
    find_package(OpenGL REQUIRED)
    list(APPEND GLVIEW_LIBS glad OpenGL::GL)
    list(APPEND GLVIEW_DEFS GLVIEW_OPENGL_ENABLED)
endif()

# Vulkan support (via volk)
if(LIGHT3D_ENABLE_VULKAN)
    list(APPEND GLVIEW_SOURCES vk_backend.cpp)
    list(APPEND GLVIEW_LIBS volk)
    list(APPEND GLVIEW_DEFS GLVIEW_VULKAN_ENABLED)
endif()

add_executable(glview ${GLVIEW_SOURCES})

target_link_libraries(glview PRIVATE ${GLVIEW_LIBS})
target_compile_definitions(glview PRIVATE ${GLVIEW_DEFS})

set_target_properties(glview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
