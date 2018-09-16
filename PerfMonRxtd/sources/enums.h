/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace pmre {
	enum sortByType {
		SORTBY_NONE,
		SORTBY_INSTANCE_NAME,
		SORTBY_RAW_COUNTER,
		SORTBY_FORMATTED_COUNTER,
		SORTBY_EXPRESSION,
		SORTBY_ROLLUP_EXPRESSION,
	};

	enum sortOrderType {
		SORTORDER_ASCENDING,
		SORTORDER_DESCENDING,
	};

	enum displayNameType {
		DISPLAYNAME_ORIGINAL,
		DISPLAYNAME_GPU_PROCESS_NAME,
		DISPLAYNAME_GPU_ENGTYPE,
	};

	enum rsltStringType {
		RESULTSTRING_NUMBER,
		RESULTSTRING_ORIGINAL_NAME,
		RESULTSTRING_UNIQUE_NAME,
		RESULTSTRING_DISPLAY_NAME,
	};

	enum rollupFunctionType {
		ROLLUP_SUM,
		ROLLUP_AVERAGE,
		ROLLUP_MINIMUM,
		ROLLUP_MAXIMUM,
		ROLLUP_COUNT,
	};

	enum getIDsType {
		GETIDS_NONE,
		GETIDS_PIDS,
		GETIDS_TIDS,
	};

	enum nameSearchPlace {
		NSP_PASSED,
		NSP_DISCARDED,
		NSP_PASSED_DISCARDED,
		NSP_DISCARDED_PASSED,
	};
}
