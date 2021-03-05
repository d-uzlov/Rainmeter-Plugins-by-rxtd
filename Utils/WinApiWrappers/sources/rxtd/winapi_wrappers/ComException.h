// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

#include "WinApiReturnCode.h"

namespace rxtd::winapi_wrappers {
	class ComException : public std::runtime_error {
		WinApiReturnCode code;
		sview source;

	public:
		ComException(index code, sview source) :
			std::runtime_error("WinAPI COM call failed"),
			code(code), source(source) { }

		[[nodiscard]]
		WinApiReturnCode getCode() const {
			return code;
		}

		[[nodiscard]]
		sview getSource() const {
			return source;
		}
	};
}
