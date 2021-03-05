// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#pragma once

namespace rxtd::perfmon {
	enum class RollupFunction {
		eSUM,
		eAVERAGE,
		eMINIMUM,
		eMAXIMUM,
		eFIRST,
	};

	enum class ResultString {
		eNUMBER,
		eORIGINAL_NAME,
		eUNIQUE_NAME,
		eDISPLAY_NAME,
	};
}

template<>
inline std::optional<rxtd::perfmon::RollupFunction> parseEnum<rxtd::perfmon::RollupFunction>(rxtd::isview name) {
	using RF = rxtd::perfmon::RollupFunction;
	if (name == L"Sum") {
		return RF::eSUM;
	} else if (name == L"Average") {
		return RF::eAVERAGE;
	} else if (name == L"Minimum") {
		return RF::eMINIMUM;
	} else if (name == L"Maximum") {
		return RF::eMAXIMUM;
	} else if (name == L"First") {
		return RF::eFIRST;
	}
	return {};
}

template<>
inline std::optional<rxtd::perfmon::ResultString> parseEnum<rxtd::perfmon::ResultString>(rxtd::isview name) {
	using RS = rxtd::perfmon::ResultString;
	if (name == L"Number") {
		return RS::eNUMBER;
	} else if (name == L"OriginalName") {
		return RS::eORIGINAL_NAME;
	} else if (name == L"UniqueName") {
		return RS::eUNIQUE_NAME;
	} else if (name == L"DisplayName") {
		return RS::eDISPLAY_NAME;
	}
	return {};
}
