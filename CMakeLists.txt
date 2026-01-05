cmake_minimum_required(VERSION 3.25..4.2 FATAL_ERROR)

project(wintab-adapter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(otdipc-headers INTERFACE)
target_include_directories(otdipc-headers INTERFACE "${CMAKE_SOURCE_DIR}/include")

add_subdirectory(src)