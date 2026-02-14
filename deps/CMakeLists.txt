# GLAD
add_subdirectory(glad)

# GLFW – disable extras we don't need
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# Volk (Vulkan meta-loader) – only when Vulkan is enabled
if(LIGHT3D_ENABLE_VULKAN)
    add_subdirectory(volk)
endif()
