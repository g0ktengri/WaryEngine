cmake_minimum_required(VERSION 3.20)
project(WaryEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SDL2_DIR "${CMAKE_SOURCE_DIR}/SDL2-2.30.10/x86_64-w64-mingw32")

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(${PROJECT_NAME} 
    ${SOURCES} 
    ${HEADERS}
    ${IMGUI_SOURCES}
    glad/src/glad.c
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    include
    ${SDL2_DIR}/include/SDL2
    glad/include
    imgui
    imgui/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${SDL2_DIR}/lib/libSDL2main.a
    ${SDL2_DIR}/lib/libSDL2.dll.a
    -lmingw32
    -lopengl32
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_DIR}/bin/SDL2.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
