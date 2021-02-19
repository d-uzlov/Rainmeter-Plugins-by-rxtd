/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::perfmon {
	enum class SortOrder {
		eASCENDING,
		eDESCENDING,
	};
}

template<>
inline std::optional<rxtd::perfmon::SortOrder> parseEnum<rxtd::perfmon::SortOrder>(rxtd::isview name) {
	using SortOrder = rxtd::perfmon::SortOrder;
	if (name == L"Descending")
		return SortOrder::eDESCENDING;
	if (name == L"Ascending")
		return SortOrder::eASCENDING;

	return {};
}
