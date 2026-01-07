// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include "utf8.hpp"

#include <Windows.h>

std::string to_utf8(const std::wstring_view w) {
  if (w.empty()) {
    return {};
  }

  const auto convert = [w](char* buffer, const std::size_t bufferSize) {
    return static_cast<std::size_t>(WideCharToMultiByte(
      CP_UTF8,
      WC_ERR_INVALID_CHARS,
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

std::wstring from_utf8(const std::string_view utf8) {
  if (utf8.empty()) {
    return {};
  }

  const auto convert = [utf8](wchar_t* buffer, const std::size_t bufferSize) {
    return static_cast<std::size_t>(MultiByteToWideChar(
      CP_UTF8,
      MB_ERR_INVALID_CHARS,
      utf8.data(),
      static_cast<DWORD>(utf8.size()),
      buffer,
      bufferSize));
  };
  const auto size = convert(nullptr, 0);
  std::wstring result;
  result.resize_and_overwrite(size, convert);
  return result;
}