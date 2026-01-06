# magic_args currently has an implicit dependency on this
find_package(magic_enum CONFIG REQUIRED)
find_package(magic_args CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

add_library(
  ForegroundOverride
  OBJECT
  ForegroundOverride.cpp ForegroundOverride.hpp
)
target_link_libraries(
  ForegroundOverride
  PUBLIC
  WIL::WIL
)

add_library(
  ForegroundOverrideDll
  MODULE
  ForegroundOverrideDll.cpp
)
target_link_libraries(
  ForegroundOverrideDll
  PRIVATE
  WIL::WIL
  minhook::minhook
  ForegroundOverride
)

add_executable(
  main
  main.cpp
  V2Server.cpp V2Server.hpp
  WintabTablet.cpp WintabTablet.hpp
  InjectDll.cpp InjectDll.hpp
)
set_target_properties(main PROPERTIES OUTPUT_NAME wintab-adapter)

target_link_libraries(
  main
  PRIVATE
  ForegroundOverride
  otdipc-headers
  magic_args::magic_args
  WIL::WIL
)

target_compile_definitions(
  main
  PRIVATE
  NOMINMAX
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
)