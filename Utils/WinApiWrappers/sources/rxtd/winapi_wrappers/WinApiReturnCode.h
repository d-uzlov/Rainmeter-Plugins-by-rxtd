// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

namespace rxtd::winapi_wrappers {
	//
	// Strongly typed WinAPI error code, that allows print functions to format error codes properly.
	// Standard error type is typedef of int, which is indistinguishable from int itself in function overload resolution.
	//
	struct WinApiReturnCode {
		index code = 0;

		WinApiReturnCode() = default;

		explicit WinApiReturnCode(index code) : code(code) {}
	};

	void writeType(std::wostream& stream, const WinApiReturnCode& code, sview options);
}
