// Copyright 2025 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include "WintabTablet.hpp"
#include <stdexcept>
#include <thread>

#include <wil/resource.h>

// These macros are part of the WinTab API; best of 1970 :)
// NOLINTBEGIN(cppcoreguidelines-macro-to-enum)
// clang-format off
#include <wintab/WINTAB.H>
#define PACKETDATA (PK_X | PK_Y | PK_Z | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_CHANGED)
#define PACKETMODE 0
#define PACKETEXPKEYS PKEXT_ABSOLUTE
#include <algorithm>
#include <print>
#include <wintab/PKTDEF.H>
// clang-format on
// NOLINTEND(cppcoreguidelines-macro-to-enum)

#define WINTAB_FUNCTIONS \
  IT(WTInfoW) \
  IT(WTOpenW) \
  IT(WTClose) \
  IT(WTOverlap) \
  IT(WTPacket)

class WintabTablet::LibWintab {
 public:
#define IT(x) decltype(&x) x = nullptr;
  WINTAB_FUNCTIONS
#undef IT

  LibWintab() : mWintab(LoadLibraryW(L"WINTAB32.dll")) {
    if (!mWintab) {
      return;
    }

#define IT(x) \
  this->x = reinterpret_cast<decltype(&::x)>(GetProcAddress(mWintab.get(), #x));
    WINTAB_FUNCTIONS
#undef IT
  }
  ~LibWintab() = default;

  operator bool() const {
    return mWintab != 0;
  }

  LibWintab(const LibWintab&) = delete;
  LibWintab(LibWintab&&) = delete;
  LibWintab& operator=(const LibWintab&) = delete;
  LibWintab& operator=(LibWintab&&) = delete;

 private:
  wil::unique_hmodule mWintab = 0;
};

namespace {
WintabTablet* gInstance {nullptr};

std::string to_utf8(const std::wstring_view w) {
  const auto convert = [w](char* buffer, const std::size_t bufferSize) {
    return static_cast<std::size_t>(WideCharToMultiByte(
      CP_UTF8,
      0,
      w.data(),
      static_cast<DWORD>(w.size()),
      buffer,
      bufferSize,
      nullptr,
      nullptr));
  };
  const auto size = convert(nullptr, 0);
  std::string result;
  result.resize_and_overwrite(size, convert);
  return result;
}
template <std::size_t N>
void to_buffer(char (&dest)[N], const std::string_view src) {
  std::ranges::fill(dest, '\0');
  std::ranges::copy_n(src.begin(), std::min(src.size(), N), dest);
}

template <std::size_t N>
void to_utf8_buffer(char (&dest)[N], const std::wstring_view src) {
  to_buffer(dest, to_utf8(src));
}
}// namespace

WintabTablet::WintabTablet(HWND window, IHandler* handler)
  : mWindow(window), mHandler(handler), mWintab(new LibWintab()) {
  if (gInstance) {
    throw std::runtime_error("Only one WintabTablet at a time!");
  }
  if (!*mWintab) {
    throw std::runtime_error("Failed to load WINTAB32.dll");
  }

  gInstance = this;

  ConnectToTablet();
}

WintabTablet::~WintabTablet() {
  gInstance = nullptr;
  if (mWintab && mContext) {
    mWintab->WTClose(std::exchange(mContext, nullptr));
  }
}

void WintabTablet::ConnectToTablet() {
  if (!mWintab) {
    return;
  }

  constexpr std::wstring_view contextName {L"OpenKneeboard"};

  LOGCONTEXTW logicalContext {};
  mWintab->WTInfoW(WTI_DEFCONTEXT, 0, &logicalContext);
  wcsncpy_s(
    static_cast<wchar_t*>(logicalContext.lcName),
    std::size(logicalContext.lcName),
    contextName.data(),
    contextName.size());
  logicalContext.lcPktData = PACKETDATA;
  logicalContext.lcMoveMask = PACKETDATA;
  logicalContext.lcPktMode = PACKETMODE;
  logicalContext.lcOptions |= CXO_MESSAGES;
  logicalContext.lcOptions &= ~static_cast<UINT>(CXO_SYSTEM);
  logicalContext.lcBtnDnMask = ~0;
  logicalContext.lcBtnUpMask = ~0;
  logicalContext.lcSysMode = false;

  AXIS axis;

  mWintab->WTInfoW(WTI_DEVICES, DVC_X, &axis);
  logicalContext.lcInOrgX = axis.axMin;
  logicalContext.lcInExtX = axis.axMax - axis.axMin;
  logicalContext.lcOutOrgX = 0;
  // From
  // https://github.com/Wacom-Developer/wacom-device-kit-windows/blob/d2bc78fe79d442a3d398f750357e46effbca1daa/Wintab%20CAD%20Test/SampleCode/CadTest.cpp#L223-L231
  //
  // This prevents outputted display-tablet coordinates
  // range from being mapped to full desktop, which
  // causes problems in multi-screen set-ups. Ie, without this
  // then the tablet coord. range is mapped to full desktop, intead
  // of only the display tablet active area.
  logicalContext.lcOutExtX = axis.axMax - axis.axMin + 2;

  mWintab->WTInfoW(WTI_DEVICES, DVC_Y, &axis);
  logicalContext.lcInOrgY = axis.axMin;
  logicalContext.lcInExtY = axis.axMax - axis.axMin;
  logicalContext.lcOutOrgY = 0;
  // same trick as above
  logicalContext.lcOutExtY = -(axis.axMax - axis.axMin + 1);

  mWintab->WTInfoW(WTI_DEVICES, DVC_NPRESSURE, &axis);

  for (UINT i = 0, tag = 0; mWintab->WTInfoW(WTI_EXTENSIONS + i, EXT_TAG, &tag);
       ++i) {
    if (tag == WTX_EXPKEYS2 || tag == WTX_OBT) {
      UINT mask = 0;
      mWintab->WTInfoW(WTI_EXTENSIONS + i, EXT_MASK, &mask);
      logicalContext.lcPktData |= mask;
      break;
    }
  }

  mContext = mWintab->WTOpenW(mWindow, &logicalContext, true);
  if (!mContext) {
    throw new std::runtime_error("Failed to open wintab tablet");
  }
  std::println("Opened wintab tablet");

  mDeviceInfo.maxX = static_cast<float>(logicalContext.lcOutExtX);
  mDeviceInfo.maxY = static_cast<float>(-logicalContext.lcOutExtY);
  mDeviceInfo.maxPressure = static_cast<uint32_t>(axis.axMax);

  // Populate name
  {
    std::wstring wide;
    wide.resize(
      mWintab->WTInfoW(WTI_DEVICES, DVC_NAME, nullptr) / sizeof(wchar_t));
    const auto actualSize
      = mWintab->WTInfoW(WTI_DEVICES, DVC_NAME, wide.data());
    to_utf8_buffer(mDeviceInfo.name, {wide.data(), actualSize - 1});
  }

  // Populate ID
  {
    std::wstring wide;
    wide.resize(
      mWintab->WTInfoW(WTI_DEVICES, DVC_PNPID, nullptr) / sizeof(wchar_t));
    const auto actualSize
      = mWintab->WTInfoW(WTI_DEVICES, DVC_PNPID, wide.data());
    std::format_to_n(
      mDeviceInfo.persistentId,
      std::size(mDeviceInfo.persistentId),
      "wintab:{}",
      to_utf8({wide.data(), actualSize - 1}));
  }

  mHandler->SetDevice(mDeviceInfo);
  ActivateContext();
}

void WintabTablet::ActivateContext() {
  mWintab->WTOverlap(mContext, TRUE);
}

bool WintabTablet::CanProcessMessage(UINT message) {
  return message == WT_PROXIMITY || message == WT_PACKET
    || message == WT_PACKETEXT || message == WT_CTXOVERLAP;
}

bool WintabTablet::ProcessMessage(
  HWND hwnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam) {
  if (!CanProcessMessage(message)) {
    return false;
  }
  if (!gInstance) {
    return false;
  }
  if (gInstance->mWindow != hwnd) {
    return false;
  }

  if (gInstance->ProcessMessageImpl(message, wParam, lParam)) {
    gInstance->mHandler->SetState(gInstance->mState);
    return true;
  }
  return false;
}

bool WintabTablet::ProcessMessageImpl(
  UINT message,
  WPARAM wParam,
  LPARAM lParam) {
  using Bits = OTDIPC::Messages::State::ValidMask;

  if (message == WT_PROXIMITY) {
    // high word indicates hardware events, low word indicates
    // context enter/leave
    mState.penIsNearSurface = (lParam & 0xffff);
    mState.validBits |= Bits::PenIsNearSurface;
    return true;
  }

  if (message == WT_CTXOVERLAP) {
    if (
      reinterpret_cast<HCTX>(wParam) == mContext
      && !(static_cast<UINT>(lParam) & CXS_ONTOP)) {
      std::println("Tablet context lost, regaining");
      ActivateContext();
    }
    return true;
  };

  // Use the context from the param instead of our stored context so we
  // can also handle messages forwarded from another window/process,
  // e.g. the game :)

  if (message == WT_PACKET) {
    PACKET packet {};
    const auto ctx = reinterpret_cast<HCTX>(lParam);
    if (!mWintab->WTPacket(ctx, static_cast<UINT>(wParam), &packet)) {
      return false;
    }
    if (packet.pkChanged & PK_X) {
      mState.x = static_cast<float>(packet.pkX);
      mState.validBits |= Bits::PositionX;
    }
    if (packet.pkChanged & PK_Y) {
      mState.y = static_cast<float>(packet.pkY);
      mState.validBits |= Bits::PositionY;
    }
    if (packet.pkChanged & PK_Z) {
      mState.hoverDistance = static_cast<float>(packet.pkZ);
      mState.validBits |= Bits::HoverDistance;
    }
    if (packet.pkChanged & PK_NORMAL_PRESSURE) {
      mState.pressure = packet.pkNormalPressure;
      mState.validBits |= Bits::Pressure;
    }
    if (packet.pkChanged & PK_BUTTONS) {
      mState.penButtons = static_cast<uint32_t>(packet.pkButtons);
      mState.validBits |= Bits::PenButtons;
    }
    return true;
  }

  if (message == WT_PACKETEXT) {
    PACKETEXT packet;
    auto ctx = reinterpret_cast<HCTX>(lParam);
    if (!mWintab->WTPacket(ctx, static_cast<UINT>(wParam), &packet)) {
      return false;
    }
    const uint16_t mask = 1ui16 << packet.pkExpKeys.nControl;
    if (packet.pkExpKeys.nState) {
      mState.auxButtons |= mask;
    } else {
      mState.auxButtons &= ~mask;
    }
    mState.validBits |= Bits::AuxButtons;
    return true;
  }

  return false;
}
