// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 buckb
// Copyright (C) 2018 Danil Uzlov

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
