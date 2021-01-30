/* 
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::perfmon::pdh {
	//
	// Strongly typed PDH error code, that allows print functions to format error codes properly.
	// Standard error type is typedef of int, which is indistinguishable from int itself in function overload resolution.
	//
	struct PdhReturnCode {
		index code;

		explicit PdhReturnCode(index code) : code(code) {}
	};

	void writeType(std::wostream& stream, const PdhReturnCode& code, sview options);
}
