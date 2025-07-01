cmake_minimum_required (VERSION 3.14)

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
  GIT_TAG        952ffddba731fc110bd50409e8d2b8a06abbd237 # latest
)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        f0569113c93ad095470c54bf34a17b36646bbbb5 # master 06.05.2025
)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.9b-docking
)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_Declare(
  proggyfonts
  GIT_REPOSITORY https://github.com/bluescan/proggyfonts.git
  GIT_TAG        v1.1.7
)

FetchContent_MakeAvailable(cmrc stb imgui glfw glm proggyfonts)

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

file(GLOB_RECURSE Common_sources 
    "./src/*.cpp" "./src/**/*.cpp"
    "./src/*.hpp" "./src/**/*.hpp"
    "./src/*.h"   "./src/**/*.h"
    "./src/shaders/*")

file(GLOB imgui_sources  "${imgui_SOURCE_DIR}/*.cpp")

source_group(TREE "${CMAKE_SOURCE_DIR}/src" FILES ${Common_sources})

add_executable(editor ${Common_sources} ${imgui_sources}
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp" 
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    "./lib/glad/src/gl.c"
    "./lib/nativefiledialog/nfd.cpp")

file(GLOB shaders RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/shaders/**")
cmrc_add_resource_library(shader_resource ALIAS cmrc_shaders NAMESPACE resources ${shaders})
cmrc_add_resource_library(font_resource ALIAS cmrc_font NAMESPACE font WHENCE "${proggyfonts_SOURCE_DIR}/ProggyVector" "${proggyfonts_SOURCE_DIR}/ProggyVector/ProggyVector-Regular.ttf")

target_include_directories(editor PRIVATE ${imgui_SOURCE_DIR} ${stb_SOURCE_DIR} "lib/glad/include" "lib/nativefiledialog" glfw ${INCLUDES})
target_link_libraries(editor PRIVATE glfw ${LIBS} cmrc_shaders cmrc_font glm)
set_target_properties(editor PROPERTIES WIN32_EXECUTABLE TRUE) 

if(MSVC)
    set(PROJECT_WARNINGS /permissive- /W4 /w14640 /MP)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PROJECT_WARNINGS -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -maes -msse4.1 -stdlib=libc++)
endif()

target_compile_options(editor PRIVATE ${PROJECT_WARNINGS})
