cmake_minimum_required(VERSION 3.16)
project(bench CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_executable(bench
  src/main.cpp
  src/fb.cpp
  src/raster.cpp
  src/render.cpp
  src/scene.cpp
  src/export_json.cpp)
target_compile_options(bench PRIVATE -Wall -Wextra)
