/*
 * Copyright (c) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "Header.hpp"

#include <string_view>

namespace OTDIPC::inline V2::Messages {
	struct DebugMessage : Header {
		static constexpr MessageType MESSAGE_TYPE = MessageType::DebugMessage;

		char data [1] {};

	  [[nodiscard]]
		std::string_view message() const
		{
			return { data, this->size - sizeof(Header) };
		}
	};
}
