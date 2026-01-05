/*
 * Copyright (c) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "MessageType.hpp"

namespace OTDIPC::inline V2::Messages {
  struct Header {
    MessageType messageType;
    uint32_t size;
    uint32_t nonPersistentTabletId;
  };
}
