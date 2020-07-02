/* 
 * Copyright (C) 2018-2020 rxtd
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
		// options
		Reference ref;
		item_t instanceIndex = 0;
		ResultString resultStringType = ResultString::eNUMBER;

		// data
		const PerfmonParent* parent = nullptr;
		const InstanceInfo* instance = nullptr;

	public:
		explicit PerfmonChild(utils::Rainmeter&& _rain);

	protected:
		void _reload() override;
		double _update() override;
		void _updateString(string& resultStringBuffer) override;
	};
}
