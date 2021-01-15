/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <winerror.h>

namespace rxtd::utils {
	class ComException : public std::runtime_error {
		HRESULT code;
		sview source;

	public:
		ComException(HRESULT code, sview source) :
			std::runtime_error("WinAPI COM call failed"),
			code(code), source(source) { }

		[[nodiscard]]
		HRESULT getCode() const {
			return code;
		}

		[[nodiscard]]
		sview getSource() const {
			return source;
		}
	};
}
