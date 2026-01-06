// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <Windows.h>
#include "ForegroundOverride.hpp"

#include <MinHook.h>
#include <concepts>
#include <format>

namespace {

template <class... Args>
void dprint(std::format_string<Args...> fmt, Args&&... args) {
  const auto msg = std::format(fmt, std::forward<Args>(args)...);
  const auto formatted = std::format("[OTD-IPC-Wintab] {}\n", msg);
  OutputDebugStringA(formatted.c_str());
}

decltype(&GetForegroundWindow) previous_GetForegroundWindow {nullptr};
HWND WINAPI hooked_GetForegroundWindow() {
  static ForegroundOverride override;
  static LARGE_INTEGER startedAt = [] {
    LARGE_INTEGER now {};
    QueryPerformanceCounter(&now);
    return now;
  }();
  static LARGE_INTEGER updateAt = startedAt;
  static LARGE_INTEGER frequency = [] {
    LARGE_INTEGER freq {};
    QueryPerformanceFrequency(&freq);
    return freq;
  }();
  static std::size_t counter {};
  ++counter;
  LARGE_INTEGER now {};
  QueryPerformanceCounter(&now);
  const auto seconds = (now.QuadPart  - startedAt.QuadPart) / frequency.QuadPart;
  if (now.QuadPart - updateAt.QuadPart > frequency.QuadPart) {
    updateAt = now;
    dprint("GetForegroundWindow called {} times - average of {} per second",
      counter,
      counter / seconds);
  }

  if (const auto value = override.Get()) {
    return value;
  }
  return previous_GetForegroundWindow();
}
static_assert(std::same_as<
              decltype(&GetForegroundWindow),
              decltype(&hooked_GetForegroundWindow)>);

void InstallHooks() {
  MH_Initialize();
  MH_CreateHook(
    reinterpret_cast<void*>(&GetForegroundWindow),
    reinterpret_cast<void*>(&hooked_GetForegroundWindow),
    reinterpret_cast<void**>(&previous_GetForegroundWindow));
  MH_EnableHook(MH_ALL_HOOKS);
}

void UninstallHooks() {
  MH_Uninitialize();
}

}// namespace

BOOL WINAPI DllMain(
  HINSTANCE const instance,
  const DWORD fdwReason,
  LPVOID const lpvReserved) {
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
      dprint("Injecting into process");
      DisableThreadLibraryCalls(instance);
      InstallHooks();
      dprint("Added hooks");
      break;
    case DLL_PROCESS_DETACH: {
      const auto terminating = static_cast<bool>(lpvReserved);
      if (!terminating) {
        UninstallHooks();
      }
      break;
    }
  }
  return TRUE;
}