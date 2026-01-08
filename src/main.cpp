// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <magic_args/magic_args.hpp>
#include <magic_enum/magic_enum.hpp>

#include "V1Server.hpp"
#include "V2Server.hpp"
#include "WintabTablet.hpp"
#include "build-config.hpp"

// clang-format off
#include <Windows.h>
#include <knownfolders.h>
#include <shlobj_core.h>
#include <wil/resource.h>
// clang-format on

namespace {
std::filesystem::path get_socket_path() {
  wil::unique_cotaskmem_string localAppData;
  if (SUCCEEDED(SHGetKnownFolderPath(
        FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
    return std::filesystem::path(localAppData.get())
      / "OpenKneeboard WinTab Adapter" / "socket";
  }
  return {};
}

static wil::unique_event gExitEvent;
BOOL WINAPI ConsoleCtrlHandler(DWORD /*dwCtrlType*/) {
  gExitEvent.SetEvent();
  return TRUE;
}

LRESULT CALLBACK WintabWndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (WintabTablet::ProcessMessage(hwnd, msg, wParam, lParam)) {
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

auto CreateWintabWindow() {
  const auto instance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));
  WNDCLASSEXW wc {sizeof(wc)};
  wc.lpfnWndProc = &WintabWndproc;
  wc.hInstance = instance, wc.lpszClassName = L"OpenKneeboard-WinTab-Adapter";

  RegisterClassExW(&wc);
  return wil::unique_hwnd {CreateWindowEx(
    0,
    wc.lpszClassName,
    nullptr,
    0,
    0,
    0,
    0,
    0,
    HWND_MESSAGE,
    nullptr,
    instance,
    nullptr)};
}

class DeviceLogger final : public IHandler {
 public:
  constexpr DeviceLogger() = default;
  constexpr DeviceLogger(IHandler* next) : mNext {next} {
  }

  ~DeviceLogger() final = default;

  void SetDevice(const OTDIPC::Messages::DeviceInfo& device) override {
    std::println(
      "Got device `{}` with persistent ID `{}`",
      device.GetName(),
      device.GetPersistentId());
    if (mNext) {
      mNext->SetDevice(device);
    }
  }

  void SetState(const OTDIPC::Messages::State& state) override {
    if (mNext) {
      mNext->SetState(state);
    }
  }

 private:
  IHandler* mNext {nullptr};
};

class MultiHandler final : public IHandler {
public:
  MultiHandler() = delete;
  ~MultiHandler() = default;

  MultiHandler(std::initializer_list<IHandler*> handlers) : mNext {handlers} {
  }

  void push_back(IHandler* handler) {
    mNext.push_back(handler);
  }

  void SetDevice(const OTDIPC::Messages::DeviceInfo& device) override {
   for (auto&& handler: mNext) {
     handler->SetDevice(device);
   }
  }
  void SetState(const OTDIPC::Messages::State& state) override {
    for (auto&& handler: mNext) {
     handler->SetState(state);
    }
  }

 private:
  std::vector<IHandler*> mNext;

};

}// namespace

struct Args {
  magic_args::flag mOtdIpcV1 {
    .help = "Support OTD-IPC v1 clients, e.g. OpenKneeboard v1.x",
  };
  magic_args::flag mOverwriteDefault {
    .help = "Overwrite the current default OTD-IPC v2 implementation, if any",
  };

  std::optional<WintabTablet::InjectableBuggyDriver> mHijackBuggyDriver;
};

MAGIC_ARGS_MAIN(Args&& args) try {
  setvbuf(stdout, nullptr, _IONBF, 0);
  gExitEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
  SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE);

  const V2Server::Config config {
    .implementationId = "com.openkneeboard.wintab-adapter",
    .humanName = "OpenKneeboard WinTab Adapter",
    .semVer = BuildConfig::SemVer,
    .debugVersion = BuildConfig::SemVer,
    .homepageUrl = "https://github.com/OpenKneeboard/wintab-adapter",
    .socketPath = get_socket_path(),
  };

  auto v2Server = V2Server(
    config,
    args.mOverwriteDefault ? V2Server::DefaultBehavior::AlwaysSet
                           : V2Server::DefaultBehavior::SetIfUnset);

  v2Server.Start();

  MultiHandler servers { &v2Server };

  std::optional<V1Server> v1Server;
  if (args.mOtdIpcV1) {
    v1Server.emplace();
    v1Server->Start();
    servers.push_back(&*v1Server);
  }

  auto handler = DeviceLogger {&servers};

  const auto window = CreateWintabWindow();
  const auto wintab
    = new WintabTablet(window.get(), &servers, args.mHijackBuggyDriver);

  const std::array events {static_cast<HANDLE>(gExitEvent.get())};
  while (true) {
    const auto InputResult = WAIT_OBJECT_0 + events.size();
    const auto result = MsgWaitForMultipleObjectsEx(
      static_cast<DWORD>(events.size()),
      events.data(),
      INFINITE,
      QS_ALLINPUT,
      MWMO_INPUTAVAILABLE);
    if (result != InputResult) {
      break;
    }
    MSG msg {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  return EXIT_SUCCESS;
} catch (const std::exception& e) {
  std::println(stderr, "Error: {}", e.what());
  return EXIT_FAILURE;
}
