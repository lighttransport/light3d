add_executable(simple_example
    simple_example.cpp
)

target_link_libraries(simple_example PRIVATE light3d)

if(EMSCRIPTEN)
    # WASM-specific settings
    set_target_properties(simple_example PROPERTIES
        OUTPUT_NAME "simple_example"
        SUFFIX ".html"
    )
    
    target_link_options(simple_example PRIVATE
        -s USE_WEBGL2=1
        -s ALLOW_MEMORY_GROWTH=1
        -s WASM=1
        --shell-file ${CMAKE_SOURCE_DIR}/examples/shell.html
    )
endif()

install(TARGETS simple_example
    RUNTIME DESTINATION bin
)
