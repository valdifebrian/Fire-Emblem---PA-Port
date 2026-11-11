cmake_minimum_required(VERSION 3.22)
project(fe9poc CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(oboe REQUIRED CONFIG)

add_library(fe9poc SHARED main.cpp)
target_compile_options(fe9poc PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(fe9poc
  log android GLESv3 EGL
  oboe::oboe)
