cmake_minimum_required(VERSION 3.16)
project(bench C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(bench
  src/main.cpp
  src/algo_demo.cpp
  src/fb.cpp
  src/raster.cpp
  src/render.cpp
  src/scene.cpp
  src/export_json.cpp)
if(MSVC)
  target_compile_options(bench PRIVATE /W4)
else()
  target_compile_options(bench PRIVATE -Wall -Wextra)
endif()

option(BUILD_VIEWER "Build the OpenGL viewer" OFF)

if(BUILD_VIEWER)
  include(FetchContent)
  FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)

  add_executable(viewer viewer/viewer.cpp deps/glad/src/gl.c)
  target_include_directories(viewer PRIVATE deps/glad/include)
  target_link_libraries(viewer PRIVATE glfw opengl32)
endif()
