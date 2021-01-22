/* 
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

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
inline std::optional<perfmon::RollupFunction> parseEnum<perfmon::RollupFunction>(isview name) {
	using RF = perfmon::RollupFunction;
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
inline std::optional<perfmon::ResultString> parseEnum<perfmon::ResultString>(isview name) {
	using RS = perfmon::ResultString;
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
