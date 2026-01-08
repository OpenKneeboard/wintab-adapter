math(EXPR BUILD_BITS "${CMAKE_SIZEOF_VOID_P} * 8")

# magic_args currently has an implicit dependency on this
find_package(magic_enum CONFIG REQUIRED)
find_package(magic_args CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

set(CODEGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CODEGEN_INCLUDE_DIR "${CODEGEN_DIR}/include")
file(MAKE_DIRECTORY "${CODEGEN_INCLUDE_DIR}")
file(
  GENERATE
  OUTPUT "${CODEGEN_INCLUDE_DIR}/build-config.hpp"
  CONTENT "
namespace BuildConfig {

constexpr auto HijackDllName = \"$<TARGET_FILE_NAME:HijackDll>\";
constexpr auto MutexName
  = L\"Local\\\\OTDIPCWintabAdapter${BUILD_BITS}.ForegroundOverride.Mutex\";
constexpr auto SHMName
  = L\"Local\\\\OTDIPCWintabAdapter${BUILD_BITS}.ForegroundOverride.HWND\";

constexpr auto SemVer = \"${PROJECT_SEMVER}\";

}
"
)
add_library(generated INTERFACE)
target_include_directories(generated INTERFACE "${CODEGEN_INCLUDE_DIR}")

add_library(
  ForegroundOverride
  OBJECT
  ForegroundOverride.cpp ForegroundOverride.hpp
)
target_link_libraries(
  ForegroundOverride
  PUBLIC
  WIL::WIL
  PRIVATE
  generated
)

add_library(
  HijackDll
  MODULE
  ForegroundOverrideDll.cpp
)
target_link_libraries(
  HijackDll
  PRIVATE
  WIL::WIL
  minhook::minhook
  ForegroundOverride
)
set_target_properties(
  HijackDll
  PROPERTIES
  OUTPUT_NAME "foreground-override-${BUILD_BITS}"
)

add_executable(
  main
  main.cpp
  V1Server.cpp V1Server.hpp
  V2Server.cpp V2Server.hpp
  WintabTablet.cpp WintabTablet.hpp
  InjectDll.cpp InjectDll.hpp
  utf8.cpp utf8.hpp
)
set_target_properties(
  main
  PROPERTIES
  OUTPUT_NAME "wintab-adapter-${BUILD_BITS}"
)

target_link_libraries(
  main
  PRIVATE
  ForegroundOverride
  generated
  otdipc-headers
  magic_args::magic_args
  WIL::WIL
  ntdll
)

target_compile_definitions(
  main
  PRIVATE
  NOMINMAX
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
)