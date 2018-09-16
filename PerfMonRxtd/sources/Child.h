/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <string>
#include "enums.h"
#include "Parent.h"

namespace pmr {
	struct TypeHolder;

	class ChildData {
	public:
		TypeHolder* typeHolder;

	private:
		wchar_t resultString[256] { };

		// read only options
		std::wstring parentName;

		// changeable options
		std::wstring instanceName;
		int instanceIndex = 0;
		int counterIndex = 0;
		pmre::rsltStringType resultStringType = pmre::RESULTSTRING_NUMBER;
		pmre::rollupFunctionType rollupFunction = pmre::ROLLUP_SUM;
		bool origName = false;
		pmre::nameSearchPlace searchPlace = pmre::nameSearchPlace::NSP_PASSED;

		// data
		ParentData* parent = nullptr;
		bool instanceNameMatchPartial = false;
		void(ChildData::* updateFunction)() = nullptr;

	public:
		explicit ChildData(TypeHolder* typeHolder);

		void reload();
		void update();

	private:
		/** returns the number of instances after whitelisting/blacklisting, matching current to previous and rollup */
		void getInstanceCount();
		/** returns the instanceName/does existence check for a given instanceIndex or instanceName
			if instanceName is not found, or instanceIndex is out of range, returns defaults */
		void getInstanceName();
		/** returns the Raw value of counterIndex for a given instanceIndex or instanceName
			if instanceName is not found, or instanceIndex is out of range, returns defaults */
		void getRawCounter();
		/** returns the Formatted value of counterIndex for a given instanceIndex or instanceName
			if instanceName is not found, or instanceIndex is out of range, returns defaults */
		void getFormattedCounter();
		/** returns the Expression value of expression[counterIndex] for a given instanceIndex or instanceName
			if instanceName is not found, or instanceIndex is out of range, returns defaults */
		void getExpression();
		/** returns the RollupExpression value of rollupExpression[counterIndex] for a given instanceIndex or instanceName
			if instanceName is not found, or instanceIndex is out of range, returns defaults */
		void getRollupExpression();
	};
}
