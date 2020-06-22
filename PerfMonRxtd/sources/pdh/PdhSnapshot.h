/* 
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <Pdh.h>
#include "OptionParser.h"

namespace rxtd::perfmon::pdh {
	using counter_t = int16_t; // because expressions?
	using item_t = int16_t;

	class PdhSnapshot {
		std::vector<std::byte> buffer;
		index counterBufferSize = 0;
		item_t itemsCount = 0;
		counter_t countersCount = 0;

	public:
		PdhSnapshot() = default;
		~PdhSnapshot() = default;

		PdhSnapshot(PdhSnapshot&& other) noexcept = default;
		PdhSnapshot& operator=(PdhSnapshot&& other) noexcept = default;

		PdhSnapshot(const PdhSnapshot& other) = delete;
		PdhSnapshot& operator=(const PdhSnapshot& other) = delete;

		item_t getItemsCount() const;

		counter_t getCountersCount() const;

		void clear();

		bool isEmpty() const;

		void setCountersCount(counter_t value);

		void setBufferSize(index size, item_t items);

		void updateSize();

		PDH_RAW_COUNTER_ITEM_W* getCounterPointer(counter_t counter);

		const PDH_RAW_COUNTER_ITEM_W* getCounterPointer(counter_t counter) const;

		const PDH_RAW_COUNTER& getItem(counter_t counter, item_t item) const;

		const wchar_t* getName(item_t item) const;

		/**
		 * in wchar_t
		 */
		index getNamesSize() const;
	};
}
