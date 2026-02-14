add_library(volk STATIC volk.c)

# volk.h includes vulkan headers — we need the Vulkan headers available.
# volk ships its own copy via VK_NO_PROTOTYPES; we just need the Vulkan
# registry headers.  Try the system first; if missing we'll error clearly.
find_path(VULKAN_HEADERS_INCLUDE_DIR
    NAMES vulkan/vulkan.h
    HINTS
        $ENV{VULKAN_SDK}/include
        /usr/include
        /usr/local/include
)

if(NOT VULKAN_HEADERS_INCLUDE_DIR)
    message(FATAL_ERROR
        "Vulkan headers (vulkan/vulkan.h) not found. "
        "Install the Vulkan SDK or vulkan-headers package.")
endif()

target_include_directories(volk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VULKAN_HEADERS_INCLUDE_DIR}
)

target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)

# volk uses dlopen on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(volk PUBLIC ${CMAKE_DL_LIBS})
endif()
