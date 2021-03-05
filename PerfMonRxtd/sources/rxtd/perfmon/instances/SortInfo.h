// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "SortBy.h"
#include "SortOrder.h"
#include "rxtd/perfmon/Reference.h"

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
