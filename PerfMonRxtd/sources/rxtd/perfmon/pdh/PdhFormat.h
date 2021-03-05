// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

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
