/* 
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "PerfmonParent.h"
#include "TypeHolder.h"

namespace rxtd::perfmon {
	class PerfmonChild : public utils::TypeHolder {
		Reference ref;
		index instanceIndex = 0;
		ResultString resultStringType = ResultString::eNUMBER;

		const PerfmonParent* parent = nullptr;
		string stringValue;

	public:
		explicit PerfmonChild(utils::Rainmeter&& _rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vUpdateString(string& resultStringBuffer) override;
	};
}
