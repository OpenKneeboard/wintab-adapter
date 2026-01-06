// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "ForegroundOverride.hpp"

#include <stdexcept>

namespace {
constexpr auto MutexName
  = L"Local\\OTDIPCWintabAdapter.ForegroundOverride.Mutex";
constexpr auto SHMName
  = L"Local\\OTDIPCWintabAdapter.ForegroundOverride.HWND";
}// namespace

ForegroundOverride::ForegroundOverride()
  : ForegroundOverride(Disposition::Read) {
}

ForegroundOverride::ForegroundOverride(HWND const window)
  : ForegroundOverride(Disposition::Write) {
  *mView = window;
}

HWND ForegroundOverride::Get() const {
  const auto lock = mMutex.acquire(nullptr, 0);
  if (lock) {
    // We got the mutex, which means the window is gone
    return nullptr;
  }

  // We failed to get the mutex, which means it's still owned, and the
  // override is still in place
  return IsWindow(*mView) ? *mView : nullptr;
}

ForegroundOverride::ForegroundOverride(const Disposition d) {
  const bool readOnly = (d == Disposition::Read);

  mMutex.reset(CreateMutexW(nullptr, FALSE, MutexName));
  THROW_LAST_ERROR_IF_NULL(mMutex);
  if (!readOnly) {
    mLock = mMutex.acquire(nullptr, 1000);
    if (!mLock) {
      throw std::runtime_error("Failed to acquire foreground window lock");
    }
  }

  // Always use PAGE_READWRITE as if the reader is first, it still
  // needs to be created as a read-write page
  mMapping.reset(CreateFileMappingW(
    INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(HWND), SHMName));
  THROW_LAST_ERROR_IF_NULL(mMapping);
  mView = static_cast<HWND*>(MapViewOfFile(
    mMapping.get(),
    readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
    0,
    0,
    sizeof(HWND)));
  THROW_LAST_ERROR_IF_NULL(mView);
}