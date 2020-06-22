/*
 * Copyright (C) 2018-2019 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PdhSnapshot.h"

#include "undef.h"

using namespace perfmon::pdh;

item_t PdhSnapshot::getItemsCount() const {
	return itemsCount;
}

counter_t PdhSnapshot::getCountersCount() const {
	return countersCount;
}

void PdhSnapshot::clear() {
	setBufferSize(0, 0);
}

bool PdhSnapshot::isEmpty() const {
	return getCountersCount() == 0 || getItemsCount() == 0;
}

void PdhSnapshot::setCountersCount(counter_t value) {
	countersCount = value;

	updateSize();
}

void PdhSnapshot::setBufferSize(index size, item_t items) {
	counterBufferSize = size;
	itemsCount = items;

	updateSize();
}

void PdhSnapshot::updateSize() {
	if (itemsCount < 1) {
		return;
	}
	buffer.reserve(countersCount * (itemsCount - 1) * sizeof(PDH_RAW_COUNTER_ITEM_W) + counterBufferSize);
}

PDH_RAW_COUNTER_ITEM_W* PdhSnapshot::getCounterPointer(counter_t counter) {
	return reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data()) + itemsCount * counter;
}

const PDH_RAW_COUNTER_ITEM_W* PdhSnapshot::getCounterPointer(counter_t counter) const {
	return reinterpret_cast<const PDH_RAW_COUNTER_ITEM_W*>(buffer.data()) + itemsCount * counter;
}

const PDH_RAW_COUNTER& PdhSnapshot::getItem(counter_t counter, item_t item) const {
	return getCounterPointer(counter)[item].RawValue;
}

const wchar_t* PdhSnapshot::getName(item_t item) const {
	return getCounterPointer(countersCount - 1)[item].szName;
}

index PdhSnapshot::getNamesSize() const {
	return (counterBufferSize - itemsCount * sizeof(PDH_RAW_COUNTER_ITEM_W)) / sizeof(wchar_t);
}
