cmake_minimum_required (VERSION 3.9)

project(Animal_Well_Editor C CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenGL REQUIRED)

if(WIN32)
    set(LIBS OpenGL32)
elseif(UNIX)
    set(LIBS GL stdc++fs pthread)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -maes -msse4.1")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw)

file(GLOB_RECURSE Common_sources 
    "./src/*.cpp" "./src/**/*.cpp"
    "./src/*.hpp" "./src/**/*.hpp"
    "./src/*.ixx" "./src/**/*.ixx"
    "./src/*.h"   "./src/**/*.h"
    "./src/shaders/*")

file(GLOB imgui_sources "./lib/imgui/*.cpp")

source_group(TREE "${CMAKE_SOURCE_DIR}/src" FILES ${Common_sources})

add_executable(editor ${Common_sources} ${imgui_sources}
    "./lib/imgui/backends/imgui_impl_opengl3.cpp" 
    "./lib/imgui/backends/imgui_impl_glfw.cpp"
    "./lib/glad/src/gl.c")

target_include_directories(editor PRIVATE "lib/imgui" "lib/stb" "lib/glm" "lib/glad/include" glfw)
target_link_libraries(editor PRIVATE glfw ${LIBS})
