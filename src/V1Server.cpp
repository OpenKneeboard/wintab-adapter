// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "V1Server.hpp"

#include <algorithm>
#include <bit>
#include <wil/resource.h>
#include "utf8.hpp"

#include <functional>
#include <print>
#include <stdexcept>

#include <OTDIPC/V1/NamedPipePath.hpp>
#include <OTDIPC/V1/Ping.hpp>

namespace {
template <std::derived_from<OTDIPC::V1::Messages::Header> T>
T CreateMessage(uint16_t vid, uint16_t pid, std::size_t size = sizeof(T)) {
  T ret {};
  ret.messageType = T::MESSAGE_TYPE;
  ret.size = static_cast<uint32_t>(size);
  ret.vid = vid;
  ret.pid = pid;
  return ret;
}

std::pair<uint16_t, uint16_t> SynthesizeVidPid(
  const std::string_view persistentId) {
  // Use FNV-1a to generate a 32-bit hash
  // Magic numbers from the algorithm
  constexpr uint32_t OffsetBasis = 0x811c9dc5;
  constexpr uint32_t Prime = 0x01000193;

  uint32_t hash = OffsetBasis;
  for (const auto c: persistentId) {
    hash ^= std::bit_cast<uint8_t>(c);
    hash *= Prime;
  }

  return {
    static_cast<uint16_t>(hash & 0xFFFF),
    static_cast<uint16_t>((hash >> 16) & 0xFFFF)};
}
}// namespace

V1Server::V1Server() = default;

V1Server::~V1Server() {
  Stop();
}

void V1Server::Start() {
  mAcceptThread = std::jthread(std::bind_front(&V1Server::AcceptLoop, this));
  mPingThread = std::jthread(std::bind_front(&V1Server::PingLoop, this));
}

void V1Server::Stop() {
  mPingThread = {};
  mAcceptThread = {};
  mPipe.reset();
}

void V1Server::AcceptLoop(const std::stop_token st) {
  while (!st.stop_requested()) {
    AcceptOnce(st);
  }
}

void V1Server::AcceptOnce(const std::stop_token st) {
  // Create named pipe instance
  wil::unique_hfile pipe(CreateNamedPipeW(
    OTDIPC::V1::NamedPipePathW,
    PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT
      | PIPE_REJECT_REMOTE_CLIENTS,
    1,// Max instances
    8192,// Out buffer size
    0,// In buffer size (outbound only)
    0,// Default timeout
    nullptr));

  if (!pipe) {
    const auto error = GetLastError();
    if (error == ERROR_ACCESS_DENIED || error == ERROR_PIPE_BUSY) {
      // Another instance is already running
      std::this_thread::sleep_for(std::chrono::seconds(1));
      return;
    }
    THROW_LAST_ERROR();
  }

  // Wait for client connection
  if (!ConnectNamedPipe(pipe.get(), nullptr)) {
    const auto error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      return;
    }
  }

  mPipe = std::move(pipe);

  // Send device info if present
  if (mV1Device.isValid) {
    Send(mV1Device);
  }

  // Keep pipe open until client disconnects or stop requested
  // For V1, we just wait - there's no ping mechanism
  while (!st.stop_requested() && mPipe) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if pipe is still connected
    DWORD bytes {};
    if (!PeekNamedPipe(mPipe.get(), nullptr, 0, nullptr, &bytes, nullptr)) {
      // Pipe disconnected
      break;
    }
  }

  mPipe.reset();
}

void V1Server::PingLoop(const std::stop_token st) {
  while (!st.stop_requested()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto msg
      = CreateMessage<OTDIPC::V1::Messages::Ping>(mV1Device.vid, mV1Device.pid);
    msg.sequenceNumber = ++mPingSequenceNumber;
    Send(msg);
  }
}

bool V1Server::SendRaw(const OTDIPC::V1::Messages::Header* data, size_t size) {
  if (data->size != size) {
    throw std::logic_error("header size mismatch");
  }

  if (!mPipe) {
    return false;
  }

  DWORD written = 0;
  if (!WriteFile(
        mPipe.get(), data, static_cast<DWORD>(size), &written, nullptr)) {
    mPipe.reset();
    return false;
  }

  return written == size;
}

void V1Server::SetDevice(const OTDIPC::V2::Messages::DeviceInfo& device) {
  const auto [vid, pid] = SynthesizeVidPid(device.GetPersistentId());

  mV1Device = CreateMessage<OTDIPC::V1::Messages::DeviceInfo>(vid, pid);
  mV1Device.isValid = true;
  mV1Device.maxX = device.maxX;
  mV1Device.maxY = device.maxY;
  mV1Device.maxPressure = device.maxPressure;

  // Copy name, ensuring null termination
  const auto name = from_utf8(device.GetName());
  std::ranges::fill(mV1Device.name, L'\0');
  std::ranges::copy_n(
    name.begin(),
    std::min(name.size(), std::size(mV1Device.name)),
    mV1Device.name);

  Send(mV1Device);
}

void V1Server::SetState(const OTDIPC::V2::Messages::State& state) {
  mV1State
    = CreateMessage<OTDIPC::V1::Messages::State>(mV1Device.vid, mV1Device.pid);

  using ValidMask = OTDIPC::V2::Messages::State::ValidMask;

  mV1State.positionValid = state.HasData(ValidMask::Position);
  if (mV1State.positionValid) {
    mV1State.x = state.x;
    mV1State.y = state.y;
  }

  mV1State.pressureValid = state.HasData(ValidMask::Pressure);
  if (mV1State.pressureValid) {
    mV1State.pressure = state.pressure;
  }

  mV1State.penButtonsValid = state.HasData(ValidMask::PenButtons);
  if (mV1State.penButtonsValid) {
    mV1State.penButtons = state.penButtons;
    // V1 requires that the pen tip is not a button
    // V2 requires that the pen tip is button 0
    mV1State.penButtons &= ~1;
  }

  mV1State.auxButtonsValid = state.HasData(ValidMask::AuxButtons);
  if (mV1State.auxButtonsValid) {
    mV1State.auxButtons = state.auxButtons;
  }

  const bool hasProximityData = state.HasData(ValidMask::PenIsNearSurface)
    || state.HasData(ValidMask::HoverDistance);
  mV1State.proximityValid = hasProximityData;
  if (hasProximityData) {
    mV1State.nearProximity = state.penIsNearSurface;
    mV1State.hoverDistance = state.hoverDistance;
  }

  Send(mV1State);
}