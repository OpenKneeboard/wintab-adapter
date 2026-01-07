  # We're going to use the 'Hybrid CRT' approach, which is the combination of the
  # UCRT and the static C++ Runtime
  #
  # https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md

  # Enable CMAKE_MSVC_RUNTIME_LIBRARY variable
  cmake_policy(SET CMP0091 NEW)
  set(
    CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )

  add_compile_options(
    "/MT$<$<CONFIG:Debug>:d>"
  )

  add_link_options(
    "/DEFAULTLIB:ucrt$<$<CONFIG:Debug>:d>.lib" # include the dynamic UCRT
    "/NODEFAULTLIB:libucrt$<$<CONFIG:Debug>:d>.lib" # remove the static UCRT
  )