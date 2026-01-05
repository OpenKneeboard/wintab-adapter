add_executable(
  main
  main.cpp
  V2Server.cpp V2Server.hpp
  WintabTablet.cpp WintabTablet.hpp
)
set_target_properties(main PROPERTIES OUTPUT_NAME wintab-adapter)

# magic_args currently has an implicit dependency on this
find_package(magic_enum CONFIG REQUIRED)
find_package(magic_args CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

target_link_libraries(
  main
  PRIVATE
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