add_library(glad STATIC src/glad.c)

target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX AND NOT APPLE)
    target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
endif()
