// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
