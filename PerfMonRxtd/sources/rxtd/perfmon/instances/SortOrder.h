// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
