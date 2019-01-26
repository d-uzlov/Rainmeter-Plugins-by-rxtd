/* 
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#undef UNIQUE_NAME

namespace rxpm {
	enum class RollupFunction {
		SUM,
		AVERAGE,
		MINIMUM,
		MAXIMUM,
		FIRST,
	};

	enum class ResultString {
		NUMBER,
		ORIGINAL_NAME,
		UNIQUE_NAME,
		DISPLAY_NAME,
	};
}
