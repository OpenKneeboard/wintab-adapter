cmake_minimum_required(VERSION 3.25..4.2 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(hybrid-crt)
include(smol-binaries)
include(sourcelink)

if (NOT PROJECT_VERSION_TWEAK)
  set(PROJECT_VERSION_TWEAK 0)
endif ()
project(wintab-adapter VERSION 1.0.3.${PROJECT_VERSION_TWEAK} LANGUAGES CXX)

set(BUILD_METADATA "" CACHE STRING "SEMVER build metadata, e.g 'gha.42' => v1.2.3+gha.42")
set(PROJECT_SEMVER "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
if (BUILD_METADATA)
  string(APPEND PROJECT_SEMVER "+${BUILD_METADATA}")
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(otdipc-headers INTERFACE)
target_include_directories(otdipc-headers INTERFACE "${CMAKE_SOURCE_DIR}/include")

add_subdirectory(src)
