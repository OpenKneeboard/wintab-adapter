// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <thread>

#include <wil/resource.h>

#include <OTDIPC/DeviceInfo.hpp>
#include <OTDIPC/State.hpp>
#include <OTDIPC/V1/DeviceInfo.hpp>
#include <OTDIPC/V1/State.hpp>
#include "IHandler.hpp"

class V1Server final : public IHandler {
 public:
  V1Server();
  ~V1Server() override;

  void Start();
  void Stop();

  void SetDevice(const OTDIPC::V2::Messages::DeviceInfo& device) override;
  void SetState(const OTDIPC::V2::Messages::State& state) override;
 private:
  void AcceptLoop(std::stop_token);
  void AcceptOnce(std::stop_token);
  void PingLoop(std::stop_token);

  bool SendRaw(const OTDIPC::V1::Messages::Header* data, size_t size);
  template <class T>
    requires(!std::is_pointer_v<T>)
  bool Send(const T& data) {
    return SendRaw(&data, sizeof(T));
  }

  std::jthread mAcceptThread;
  std::jthread mPingThread;

  wil::unique_hfile mPipe;

  OTDIPC::V1::Messages::DeviceInfo mV1Device {};
  OTDIPC::V1::Messages::State mV1State {};

  std::size_t mPingSequenceNumber {};
};
