// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <OTDIPC/DeviceInfo.hpp>
#include <OTDIPC/State.hpp>

struct IHandler {
  virtual ~IHandler() = default;

  virtual void SetDevice(const OTDIPC::Messages::DeviceInfo& device) = 0;
  virtual void SetState(const OTDIPC::Messages::State& state) = 0;
};