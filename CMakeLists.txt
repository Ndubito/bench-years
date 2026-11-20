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

option(BUILD_OPENGL_VIEWER "Build the OpenGL 3D scene viewer" ON)
if(BUILD_OPENGL_VIEWER)
  include(FetchContent)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip)
  FetchContent_MakeAvailable(glfw)
  find_package(OpenGL REQUIRED)

  add_library(glad STATIC deps/glad/src/gl.c)
  target_include_directories(glad PUBLIC deps/glad/include)

  add_executable(bench3d src/viewer3d.cpp)
  target_link_libraries(bench3d PRIVATE glad glfw OpenGL::GL)
endif()
