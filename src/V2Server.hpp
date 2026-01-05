// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <OTDIPC/DeviceInfo.hpp>
#include <OTDIPC/State.hpp>
#include "IHandler.hpp"

// clang-format off
#include <Windows.h>
#include <winsock2.h>// needed for wil/resource.h to define unique_socket
#include <wil/resource.h>
// clang-format on


class V2Server final : public IHandler {
 public:
  struct Config {
    std::string implementationId;// e.g. "com.example.driver"
    std::string humanName;// e.g. "My Custom Driver"
    std::string semVer;// e.g. "1.0.0"
    std::string debugVersion;// e.g. "v1.0.0 (Build 42)"
    std::string homepageUrl;// e.g. "https://example.com"
    std::filesystem::path socketPath;// Absolute path for the socket
  };

  enum class DefaultBehavior {
    SetIfUnset,
    DoNotSet,
    AlwaysSet,
  };

  V2Server() = delete;
  explicit V2Server(
    Config config,
    DefaultBehavior = DefaultBehavior::SetIfUnset);

  ~V2Server() override;

  void Start();
  void Stop();

  void SetDevice(const OTDIPC::Messages::DeviceInfo& device) override;
  void SetState(const OTDIPC::Messages::State& state) override;
  void SendDebugMessage(std::string_view message);

 private:
  void AcceptLoop(std::stop_token);
  void AcceptOnce(std::stop_token);
  void PingLoop(std::stop_token);

  bool SendRaw(const void* data, size_t size);
  template <class T>
    requires(!std::is_pointer_v<T>)
  bool Send(const T& data) {
    return SendRaw(&data, sizeof(T));
  }

  void PublishDiscovery();
  void EnsureDefaultExists();

  Config mConfig {};
  DefaultBehavior mDefaultBehavior {};

  std::jthread mAcceptThread;
  std::jthread mPingThread;

  std::optional<OTDIPC::Messages::DeviceInfo> mDevice;

  wil::unique_socket mListenSocket;
  wil::unique_socket mClientSocket;
};
