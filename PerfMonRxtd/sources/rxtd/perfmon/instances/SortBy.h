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
	enum class SortBy {
		eNONE,
		eINSTANCE_NAME,
		eVALUE,
	};
}

template<>
inline std::optional<rxtd::perfmon::SortBy> parseEnum<rxtd::perfmon::SortBy>(rxtd::isview name) {
	using SortBy = rxtd::perfmon::SortBy;
	if (name == L"None")
		return SortBy::eNONE;
	if (name == L"InstanceName")
		return SortBy::eINSTANCE_NAME;
	if (name == L"Value")
		return SortBy::eVALUE;

	return {};
}
