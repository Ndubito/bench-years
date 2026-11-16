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


if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()