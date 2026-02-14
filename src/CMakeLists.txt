add_library(light3d
    engine.cpp
    renderer_opengl.cpp
    math.cpp
    prim.cpp
    xform.cpp
    stage.cpp
    mesh_data.cpp
    skeleton.cpp
    skel_animation.cpp
    timeline.cpp
)

target_include_directories(light3d PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(light3d PUBLIC cxx_std_17)

# Platform-specific sources and linking
if(EMSCRIPTEN)
    target_sources(light3d PRIVATE
        renderer_webgl.cpp
    )
    target_compile_options(light3d PRIVATE
        -s USE_WEBGL2=1
    )
    target_link_options(light3d PUBLIC
        -s USE_WEBGL2=1
        -s ALLOW_MEMORY_GROWTH=1
    )
elseif(APPLE)
    if(LIGHT3D_ENABLE_METAL)
        target_sources(light3d PRIVATE
            renderer_metal.mm
        )
        target_link_libraries(light3d PUBLIC
            "-framework Metal"
            "-framework MetalKit"
            "-framework Foundation"
            "-framework QuartzCore"
        )
    endif()
elseif(UNIX)
    # Linux-specific
    if(LIGHT3D_ENABLE_OPENGL)
        find_package(OpenGL)
        if(OPENGL_FOUND)
            target_link_libraries(light3d PUBLIC OpenGL::GL)
        else()
            message(WARNING "OpenGL not found. OpenGL backend will not be available.")
        endif()
    endif()
elseif(WIN32)
    # Windows-specific
    if(LIGHT3D_ENABLE_OPENGL)
        target_link_libraries(light3d PUBLIC opengl32)
    endif()
endif()

# Vulkan support
if(LIGHT3D_ENABLE_VULKAN)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        target_sources(light3d PRIVATE renderer_vulkan.cpp)
        target_link_libraries(light3d PUBLIC Vulkan::Vulkan)
        target_compile_definitions(light3d PUBLIC LIGHT3D_VULKAN_ENABLED)
    else()
        message(WARNING "Vulkan not found. Vulkan backend will not be available.")
    endif()
endif()

# C++20 coroutine support
if(LIGHT3D_ENABLE_CPP20)
    target_compile_features(light3d PUBLIC cxx_std_20)
    target_compile_definitions(light3d PUBLIC LIGHT3D_CPP20_ENABLED)
endif()

# Export compile definitions
if(LIGHT3D_ENABLE_OPENGL)
    target_compile_definitions(light3d PUBLIC LIGHT3D_OPENGL_ENABLED)
endif()
if(LIGHT3D_ENABLE_METAL)
    target_compile_definitions(light3d PUBLIC LIGHT3D_METAL_ENABLED)
endif()
if(LIGHT3D_ENABLE_WEBGPU)
    target_compile_definitions(light3d PUBLIC LIGHT3D_WEBGPU_ENABLED)
endif()

# Installation
install(TARGETS light3d
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/light3d
    DESTINATION include
)
