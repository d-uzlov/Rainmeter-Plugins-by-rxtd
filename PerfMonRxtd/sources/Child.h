/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "enums.h"
#include "Parent.h"

namespace pmr {
	struct TypeHolder;

	class ChildData {
		TypeHolder* const typeHolder;
		wchar_t resultString[256] { };

		// options
		pmrexp::reference ref;
		int instanceIndex = 0;
		pmre::ResultString resultStringType = pmre::ResultString::NUMBER;

		// data
		const ParentData* parent = nullptr;

	public:
		explicit ChildData(TypeHolder* typeHolder);

		void reload();
		void update();
	};

	extern std::vector<ParentData*> parentMeasuresVector;
}
