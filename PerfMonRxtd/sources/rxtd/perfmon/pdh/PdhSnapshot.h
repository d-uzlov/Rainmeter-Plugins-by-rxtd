/* 
 * Copyright (C) 2018-2021 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include "my-pdh.h"

namespace rxtd::perfmon::pdh {
	class PdhSnapshot : MovableOnlyBase {
		std::vector<std::byte> buffer;
		index counterBufferSize = 0;
		index itemsCount = 0;
		index countersCount = 0;

	public:
		[[nodiscard]]
		auto getItemsCount() const {
			return itemsCount;
		}

		[[nodiscard]]
		index getCountersCount() const {
			return countersCount;
		}

		void clear() {
			setBufferSize(0, 0);
		}

		[[nodiscard]]
		bool isEmpty() const {
			return countersCount == 0 || getItemsCount() == 0;
		}

		void setCountersCount(index value) {
			countersCount = value;

			updateSize();
		}

		void setBufferSize(index size, index items) {
			counterBufferSize = size;
			itemsCount = items;

			updateSize();
		}

		void updateSize() {
			if (itemsCount < 1) {
				return;
			}
			buffer.resize(countersCount * (itemsCount - 1) * sizeof(PDH_RAW_COUNTER_ITEM_W) + counterBufferSize);
		}

		[[nodiscard]]
		PDH_RAW_COUNTER_ITEM_W* getCounterPointer(index counter) {
			return reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data()) + itemsCount * counter;
		}

		[[nodiscard]]
		const PDH_RAW_COUNTER_ITEM_W* getCounterPointer(index counter) const {
			return reinterpret_cast<const PDH_RAW_COUNTER_ITEM_W*>(buffer.data()) + itemsCount * counter;
		}

		[[nodiscard]]
		const PDH_RAW_COUNTER& getItem(index counter, index item) const {
			return getCounterPointer(counter)[item].RawValue;
		}

		[[nodiscard]]
		sview getName(index item) const {
			return getCounterPointer(countersCount - 1)[item].szName;
		}

		/**
		 * in wchar_t
		 */
		[[nodiscard]]
		index getNamesSize() const {
			return (counterBufferSize - itemsCount * static_cast<index>(sizeof(PDH_RAW_COUNTER_ITEM_W))) / static_cast<index>(sizeof(wchar_t));
		}
	};
}
