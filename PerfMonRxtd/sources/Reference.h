/* 
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "enums.h"
#include "StringUtils.h"

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
