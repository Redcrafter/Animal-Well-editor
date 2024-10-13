cmake_minimum_required (VERSION 3.9)

project(Animal_Well_Editor C CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
set(OpenGL_GL_PREFERENCE "GLVND")

cmake_policy(SET CMP0069 NEW) 
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

include(FetchContent)
FetchContent_Declare(
  cmrc
  GIT_REPOSITORY https://github.com/vector-of-bool/cmrc.git
  GIT_TAG        2.0.1
)
FetchContent_MakeAvailable(cmrc)

find_package(OpenGL REQUIRED)

if(WIN32)
    set(LIBS OpenGL32)
elseif(UNIX)
    find_package(GTK2 REQUIRED gtk)

    set(INCLUDES ${GTK2_INCLUDE_DIRS})
    set(LIBS GL ${GTK2_LIBRARIES} stdc++fs pthread)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -maes -msse4.1")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw)

file(GLOB_RECURSE Common_sources 
    "./src/*.cpp" "./src/**/*.cpp"
    "./src/*.hpp" "./src/**/*.hpp"
    "./src/*.h"   "./src/**/*.h"
    "./src/shaders/*")

file(GLOB imgui_sources "./lib/imgui/*.cpp")

source_group(TREE "${CMAKE_SOURCE_DIR}/src" FILES ${Common_sources})

add_executable(editor ${Common_sources} ${imgui_sources}
    "./lib/imgui/backends/imgui_impl_opengl3.cpp" 
    "./lib/imgui/backends/imgui_impl_glfw.cpp"
    "./lib/imgui/misc/cpp/imgui_stdlib.cpp"
    "./lib/glad/src/gl.c"
    "./lib/nativefiledialog/nfd.cpp")

file(GLOB shaders RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/shaders/**")
cmrc_add_resource_library(shader_resource ALIAS cmrc_shaders NAMESPACE resources ${shaders} "lib/proggyfonts/ProggyVector/ProggyVector-Regular.ttf")

target_include_directories(editor PRIVATE "lib/imgui" "lib/stb" "lib/glm" "lib/glad/include" "lib/nativefiledialog" glfw ${INCLUDES})
target_link_libraries(editor PRIVATE glfw ${LIBS} cmrc_shaders)
set_target_properties(editor PROPERTIES WIN32_EXECUTABLE TRUE) 

if(MSVC)
    set(PROJECT_WARNINGS /permissive- /W4 /w14640)
elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_WARNINGS -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic)
endif()

target_compile_options(editor PRIVATE ${PROJECT_WARNINGS})
