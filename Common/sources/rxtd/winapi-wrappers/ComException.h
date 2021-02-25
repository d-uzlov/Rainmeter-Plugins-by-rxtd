/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "WinApiReturnCode.h"

namespace rxtd::common::winapi_wrappers {
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
