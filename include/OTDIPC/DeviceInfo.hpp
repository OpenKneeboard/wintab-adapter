/*
 * Copyright (c) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "Header.hpp"

#include <string_view>

namespace OTDIPC::inline V2::Messages {

struct DeviceInfo : Header {
  static constexpr MessageType MESSAGE_TYPE = MessageType::DeviceInfo;
  static constexpr std::size_t PersistentIdMaxLength = 256;
  static constexpr std::size_t NameMaxLength = 256;

  constexpr DeviceInfo() : Header {MESSAGE_TYPE, sizeof(*this)} {
  }

  float maxX {};
  float maxY {};
  uint32_t maxPressure {};
  uint16_t vendorId {};// Deprecated: use persistentId instead. Used to allow
                       // matching with other data sources
  uint16_t productId {};// Deprecated: use persistentId instead. Used to allow
                        // matching with other data sources

  char persistentId[PersistentIdMaxLength] {};
  char name[NameMaxLength] {};

  constexpr std::string_view GetName() const {
    const auto end = std::ranges::find(name, '\0');
    return {name, end};
  }

  constexpr std::string_view GetPersistentId() const {
    const auto end = std::ranges::find(persistentId, '\0');
    return {persistentId, end};
  }
};

}// namespace OTDIPC::inline V2::Messages
