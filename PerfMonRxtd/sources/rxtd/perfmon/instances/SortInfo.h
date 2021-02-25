/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "rxtd/perfmon/Reference.h"
#include "SortBy.h"
#include "SortOrder.h"

namespace rxtd::perfmon {
	struct SortInfo {
		SortBy sortBy = SortBy::eNONE;
		SortOrder sortOrder = SortOrder::eDESCENDING;

		struct {
			index sortIndex = 0;
			Reference::Type expressionType{};
			RollupFunction sortRollupFunction{};
		} sortByValueInformation;
	};
}
