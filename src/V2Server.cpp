// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT
#include "V2Server.hpp"

#include <afunix.h>
#include <expected>
#include <shlobj.h>
#include <wil/filesystem.h>
#include <wil/result.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <print>
#include <variant>

#include <OTDIPC/DebugMessage.hpp>
#include <OTDIPC/Hello.hpp>
#include <OTDIPC/Ping.hpp>

#pragma comment(lib, "ws2_32.lib")

namespace {

constexpr uint64_t ProtocolVersion = 0x02'20260205'01;
constexpr uint8_t CompatibilityVersion = 1;

template<std::size_t N>
std::string_view TruncateNulls(const char (&in)[N]) {
  return std::string_view(in, strnlen(in, N));
}

// Helper to get LocalAppData/otd-ipc/servers/v2
std::filesystem::path GetDiscoveryDir() {
  wil::unique_cotaskmem_string localAppData;
  if (SUCCEEDED(SHGetKnownFolderPath(
        FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
    std::filesystem::path path(localAppData.get());
    return path / "otd-ipc" / "servers" / "v2";
  }
  throw std::runtime_error("Failed to resolve %LOCALAPPDATA%");
}

template <std::derived_from<OTDIPC::Messages::Header> T>
void InitHeader(T& msg, uint32_t tabletId, std::size_t size = sizeof(T)) {
  msg.messageType = T::MESSAGE_TYPE;
  msg.size = static_cast<uint32_t>(size);
  msg.nonPersistentTabletId = tabletId;
}

struct socket_closed_t {};
struct wsa_error_t {
  int value {};
};
inline constexpr socket_closed_t socket_closed;

std::expected<void, std::variant<socket_closed_t, wsa_error_t>>
ReadNBytes(SOCKET socket, void* const buffer, const std::size_t count) {
  auto p = buffer;
  auto toRead = count;
  while (toRead > 0) {
    const int result
      = recv(socket, static_cast<char*>(p), static_cast<int>(toRead), 0);
    if (result == 0) {
      return std::unexpected {socket_closed};
    }
    if (result == SOCKET_ERROR) {
      const auto error = WSAGetLastError();
      if (error == WSAECONNRESET) {
        return std::unexpected {socket_closed};
      }
      return std::unexpected {wsa_error_t {error}};
    }
    toRead -= result;
  }
  return {};
}

template <std::size_t N>
void CopyTo(char (&dest)[N], const std::string_view src) {
  std::ranges::fill(dest, '\0');
  std::ranges::copy_n(src.begin(), std::min(src.size(), N), dest);
}

}// namespace

V2Server::V2Server(Config config, const DefaultBehavior defaultBehavior)
  : mConfig(std::move(config)), mDefaultBehavior(defaultBehavior) {
  // Initialize WinSock
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    THROW_HR(HRESULT_FROM_WIN32(result));
  }
}

V2Server::~V2Server() {
  Stop();
  WSACleanup();
}

void V2Server::Start() {
  // 1. Setup Socket
  mListenSocket.reset(socket(AF_UNIX, SOCK_STREAM, 0));
  if (!mListenSocket) {
    THROW_LAST_ERROR();
  }

  // Delete existing socket file if it exists (Unix socket requirement)
  std::error_code ec;
  if (std::filesystem::exists(mConfig.socketPath, ec)) {
    std::filesystem::remove(mConfig.socketPath, ec);
  }

  // Ensure parent directory exists for socket
  std::filesystem::create_directories(mConfig.socketPath.parent_path(), ec);
  // `exists()` is unusable on Unix sockets on Windows, so unconditionally
  // remove and ignore error https://github.com/microsoft/STL/issues/4077
  std::filesystem::remove(mConfig.socketPath, ec);

  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;

  // Copy path to sun_path (max 108 chars usually)
  std::string pathStr = mConfig.socketPath.string();
  if (pathStr.length() >= sizeof(addr.sun_path)) {
    throw std::runtime_error("Socket path is too long for AF_UNIX");
  }
  strncpy_s(addr.sun_path, pathStr.c_str(), _TRUNCATE);

  if (
    bind(mListenSocket.get(), (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
    THROW_LAST_ERROR();
  }

  if (listen(mListenSocket.get(), 1) == SOCKET_ERROR) {
    THROW_LAST_ERROR();
  }

  PublishDiscovery();

  mAcceptThread = std::jthread(std::bind_front(&V2Server::AcceptLoop, this));
  mPingThread = std::jthread(std::bind_front(&V2Server::PingLoop, this));
}

void V2Server::PingLoop(const std::stop_token st) {
  while (!st.stop_requested()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (st.stop_requested()) {
      break;
    }

    if (!mClientSocket) {
      continue;
    }

    static uint64_t seq = 0;
    OTDIPC::Messages::Ping ping = {};
    InitHeader(ping, 0);
    ping.sequenceNumber = ++seq;
    Send(ping);
  }
}

void V2Server::Stop() {
  mListenSocket.reset();
  mPingThread = {};
  mAcceptThread = {};
}

void V2Server::AcceptLoop(const std::stop_token st) {
  while (!st.stop_requested()) {
    AcceptOnce(st);
  }
  mListenSocket.reset();
}

void V2Server::AcceptOnce(const std::stop_token st) {
  // Block waiting for a client
  wil::unique_socket client(accept(mListenSocket.get(), nullptr, nullptr));

  if (!client) {
    // accept failed (likely Stop() called and socket closed)
    return;
  }

  mClientSocket = std::move(client);

  // HANDSHAKE PHASE

  OTDIPC::Messages::Hello hello {
    .protocolVersion = ProtocolVersion,
    .compatibilityVersion = CompatibilityVersion,
  };
  CopyTo(hello.humanReadableName, mConfig.humanName);
  CopyTo(hello.humanReadableVersion, mConfig.humanVersion);
  CopyTo(hello.implementationID, mConfig.implementationId);
  SendRaw(&hello.header, sizeof(hello));

  if (mDevice.has_value()) {
    Send(*mDevice);
  }

  // OPERATIONAL PHASE

  std::vector<std::byte> buffer;
  buffer.resize(1024);
  while (!st.stop_requested()) {
    struct ReadErrorVisitor {
      static void operator()(socket_closed_t) {
        std::println("Client has disconnected");
      }
      static void operator()(const wsa_error_t error) {
        std::println(
          stderr,
          "Reading from client failed with {:#010x}",
          static_cast<uint32_t>(HRESULT_FROM_WIN32(error.value)));
      }
    };

    auto it = buffer.data();
    auto header = reinterpret_cast<OTDIPC::Messages::Header*>(buffer.data());
    if (const auto ok = ReadNBytes(mClientSocket.get(), it, sizeof(*header));
        !ok) {
      std::visit(ReadErrorVisitor {}, ok.error());
      return;
    }

    if (header->size > buffer.size()) {
      buffer.resize(header->size);
      header = reinterpret_cast<OTDIPC::Messages::Header*>(buffer.data());
      it = buffer.data();
    }
    it += sizeof(*header);

    if (const auto ok
        = ReadNBytes(mClientSocket.get(), it, header->size - sizeof(*header));
        !ok) {
      std::visit(ReadErrorVisitor {}, ok.error());
      return;
    }

    if (header->messageType == OTDIPC::Messages::DebugMessage::MESSAGE_TYPE) {
      std::println(
        "Client message: {}",
        reinterpret_cast<OTDIPC::Messages::DebugMessage*>(header)->message());
      continue;
    }

    if (header->messageType == OTDIPC::Messages::Hello::MESSAGE_TYPE) {
      const auto& hello = *reinterpret_cast<OTDIPC::Messages::Hello*>(header);
      std::println("Client hello: {} {} (proto {:#x}, ID '{}'/ cv {})",
          TruncateNulls(hello.humanReadableName),
          TruncateNulls(hello.humanReadableVersion),
          hello.protocolVersion,
          TruncateNulls(hello.implementationID),
          hello.compatibilityVersion);
      continue;
    }

    std::println(
      stderr,
      "Received unexpected client message type {}",
      std::to_underlying(header->messageType));
  }
  mClientSocket.reset();
}

void V2Server::PublishDiscovery() {
  const auto root = GetDiscoveryDir();
  std::filesystem::create_directories(root / "available");

  // 1. Write Metadata
  const auto metaPath
    = root / "available" / std::format("{}.txt", mConfig.implementationId);
  std::ofstream meta(metaPath);

  std::string socketPathGeneric = absolute(mConfig.socketPath).generic_string();

  meta << "ID=" << mConfig.implementationId << "\n";
  meta << "SOCKET=" << socketPathGeneric << "\n";
  meta << "HUMAN_READABLE_NAME=" << mConfig.humanName << "\n";
  meta << "HUMAN_READABLE_VERSION=" << mConfig.humanVersion << "\n";
  meta << "COMPATIBILITY_VERSION=" << CompatibilityVersion << "\n";
  meta << "HOMEPAGE=" << mConfig.homepageUrl << "\n";
  meta.close();

  // 2. Handle Default
  EnsureDefaultExists();
}

void V2Server::EnsureDefaultExists() {
  if (mDefaultBehavior == DefaultBehavior::DoNotSet)
    return;

  const auto root = GetDiscoveryDir();
  const auto defaultPath = root / "default.txt";

  if (
    mDefaultBehavior == DefaultBehavior::AlwaysSet
    || !std::filesystem::exists(defaultPath)) {
    std::ofstream def(defaultPath);
    def << mConfig.implementationId;
  }
}

bool V2Server::SendRaw(const OTDIPC::Messages::Header* data, size_t size) {
  if (data->size != size)
    throw std::runtime_error("Header size mismatch");
  if (!mClientSocket)
    return false;

  const int result = send(
    mClientSocket.get(),
    reinterpret_cast<const char*>(data),
    static_cast<int>(size),
    0);
  if (result == SOCKET_ERROR) {
    mClientSocket.reset();
    return false;
  }
  return true;
}

void V2Server::SetDevice(const OTDIPC::Messages::DeviceInfo& device) {
  mDevice = device;
  InitHeader(*mDevice, device.nonPersistentTabletId);
  ;

  Send(*mDevice);
}

void V2Server::SetState(const OTDIPC::Messages::State& state) {
  // Copy state to modify header
  OTDIPC::Messages::State msg = state;
  InitHeader(msg, mDevice->nonPersistentTabletId);

  Send(msg);
}

void V2Server::SendDebugMessage(std::string_view message) {
  // Calculate total size: Header + Message Body
  const size_t totalSize = sizeof(OTDIPC::Messages::Header) + message.size();

  std::vector<char> buffer(totalSize);
  InitHeader(
    *reinterpret_cast<OTDIPC::Messages::DebugMessage*>(buffer.data()),
    0,
    totalSize);

  // Copy string data immediately after header
  std::memcpy(
    buffer.data() + sizeof(OTDIPC::Messages::Header),
    message.data(),
    message.size());

  SendRaw(
    reinterpret_cast<const OTDIPC::Messages::Header*>(buffer.data()),
    totalSize);
}
