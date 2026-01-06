// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>

void InjectDll(uint32_t processID, const std::filesystem::path& dllPath);

void InjectDllByExecutableFileName(
  std::wstring_view executableFileName,
  const std::filesystem::path& dllPath);