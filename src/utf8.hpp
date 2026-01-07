// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

std::string to_utf8(std::wstring_view);
std::wstring from_utf8(std::string_view);