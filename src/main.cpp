// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include <magic_args/magic_args.hpp>
#include "V2Server.hpp"

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

}// namespace

struct Args {
  magic_args::option<bool> mOverwriteDefault {
    .help = "Overwrite the current default OTD-IPC v2 implementation, if any",
  };
};

MAGIC_ARGS_MAIN(Args&& args) {
  const V2Server::Config config {
    .implementationId = "com.openkneeboard.wintab-adapter",
    .humanName = "OpenKneeboard WinTab Adapter",
    .semVer = "0.0.1+master",
    .debugVersion = "v0.1 master",
    .homepageUrl = "https://github.com/OpenKneeboard/wintab-adapter",
    .socketPath = get_socket_path(),
  };

  auto server = V2Server(
    config,
    args.mOverwriteDefault ? V2Server::DefaultBehavior::AlwaysSet
                           : V2Server::DefaultBehavior::SetIfUnset);

  server.Start();

  gExitEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
  SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE);

  std::ignore = gExitEvent.wait();

  return EXIT_SUCCESS;
}
