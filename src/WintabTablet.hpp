// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include "ForegroundOverride.hpp"
#include "IHandler.hpp"

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

struct HCTX__;

class WintabTablet final {
 public:
  enum class InjectableBuggyDriver {
    Huion,
    HuionAlternate,
    Gaomon,
    XPPen,
  };

  WintabTablet() = delete;
  WintabTablet(
    HWND window,
    IHandler* handler,
    std::optional<InjectableBuggyDriver>);
  ~WintabTablet();

  [[nodiscard]]
  static bool
  ProcessMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

 private:
  class LibWintab;

  HWND mWindow {nullptr};
  ForegroundOverride mForegroundOverride;
  IHandler* mHandler {nullptr};
  std::unique_ptr<LibWintab> mWintab;
  HCTX__* mContext {nullptr};

  std::uint32_t mNextTabletID {1};

  OTDIPC::Messages::DeviceInfo mDeviceInfo {};
  OTDIPC::Messages::State mState {};

  void ActivateContext();

  void ConnectToTablet();
  [[nodiscard]]
  static bool CanProcessMessage(UINT message);
  [[nodiscard]]
  bool ProcessMessageImpl(UINT message, WPARAM wParam, LPARAM lParam);
};
