set(GLVIEW_SOURCES main.cpp)
set(GLVIEW_LIBS light3d glfw)
set(GLVIEW_DEFS "")

option(LIGHT3D_EXAMPLE_USE_LIGHTUSD_C "Enable USD loading via lightusd-c" OFF)

if(LIGHT3D_EXAMPLE_USE_LIGHTUSD_C)
    set(TINYUSDZ_ROOT_DIR "${CMAKE_SOURCE_DIR}/deps/tinyusdz")
    
    # We need tinyusdz targets
    if(NOT TARGET tinyusdz_static)
        set(TINYUSDZ_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(TINYUSDZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(TINYUSDZ_WITH_PYTHON OFF CACHE BOOL "" FORCE)
        add_subdirectory("${TINYUSDZ_ROOT_DIR}" tinyusdz_build EXCLUDE_FROM_ALL)
    endif()

    set(LUSD_TINYUSDZ_DIR "${TINYUSDZ_ROOT_DIR}" CACHE PATH "" FORCE)
    # The user provided path is deps/tinyusdz/sandbox/lightusd-c
    add_subdirectory("${TINYUSDZ_ROOT_DIR}/sandbox/lightusd-c" lightusd_c_build EXCLUDE_FROM_ALL)
    
    list(APPEND GLVIEW_LIBS lightusd-c lydra-c)
    list(APPEND GLVIEW_DEFS LIGHT3D_EXAMPLE_USE_LIGHTUSD_C)
endif()

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
