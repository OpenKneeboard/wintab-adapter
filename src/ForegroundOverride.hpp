// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <wil/resource.h>
#include <Windows.h>

class ForegroundOverride final {
public:
  // Read-only
  ForegroundOverride();
  // Write-only
  explicit ForegroundOverride(HWND window);

  // May return nullptr
  [[nodiscard]]
  HWND Get() const;

private:
  enum class Disposition {
    Read,
    Write
  };
  explicit ForegroundOverride(Disposition);

  wil::unique_mutex mMutex;
  wil::unique_handle mMapping;
  wil::mutex_release_scope_exit mLock;
  HWND* mView { nullptr };
};