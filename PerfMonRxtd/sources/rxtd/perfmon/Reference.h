// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#pragma once
#include "enums.h"
#include "MatchPattern.h"

namespace rxtd::perfmon {
	struct Reference {
		enum class Type : uint8_t {
			eCOUNTER_RAW,
			eCOUNTER_FORMATTED,
			eEXPRESSION,
			eROLLUP_EXPRESSION,
			eCOUNT,
		};

		MatchPattern namePattern;
		index counter = 0;
		RollupFunction rollupFunction{};
		Type type{};
		bool discarded = false;
		bool useOrigName = false;
		bool total = false;
	};
}
