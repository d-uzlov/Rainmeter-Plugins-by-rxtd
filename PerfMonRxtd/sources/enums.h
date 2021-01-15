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
