/*
 * Copyright (C) 2018-2019 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PdhWrapper.h"
#include <PdhMsg.h>

#include "undef.h"

#pragma comment(lib, "pdh.lib")

using namespace perfmon::pdh;

Snapshot::Snapshot() = default;
Snapshot::~Snapshot() = default;
Snapshot::Snapshot(Snapshot&& other) noexcept = default;
Snapshot& Snapshot::operator=(Snapshot&& other) noexcept = default;

index Snapshot::getItemsCount() const {
	return itemsCount;
}

index Snapshot::getCountersCount() const {
	return countersCount;
}

void Snapshot::clear() {
	setBufferSize(0, 0);
}

bool Snapshot::isEmpty() const {
	return getCountersCount() == 0 || getItemsCount() == 0;
}

void Snapshot::setCountersCount(index value) {
	countersCount = value;

	updateSize();
}

void Snapshot::setBufferSize(index size, index items) {
	counterBufferSize = size;
	itemsCount = items;

	updateSize();
}

void Snapshot::updateSize() {
	if (itemsCount < 1) {
		return;
	}
	buffer.reserve(countersCount * (itemsCount - 1) * sizeof(PDH_RAW_COUNTER_ITEM_W) + counterBufferSize);
}

PDH_RAW_COUNTER_ITEM_W* Snapshot::getCounterPointer(index counter) {
	return reinterpret_cast<PDH_RAW_COUNTER_ITEM_W*>(buffer.data()) + itemsCount * counter;
}

const PDH_RAW_COUNTER_ITEM_W* Snapshot::getCounterPointer(index counter) const {
	return reinterpret_cast<const PDH_RAW_COUNTER_ITEM_W*>(buffer.data()) + itemsCount * counter;
}

const PDH_RAW_COUNTER& Snapshot::getItem(index counter, index index) const {
	return getCounterPointer(counter)[index].RawValue;
}

const wchar_t* Snapshot::getName(index index) const {
	return getCounterPointer(countersCount - 1)[index].szName;
}

index Snapshot::getNamesSize() const {
	return (counterBufferSize - itemsCount * sizeof(PDH_RAW_COUNTER_ITEM_W)) / sizeof(wchar_t);
}

PdhWrapper::QueryWrapper::~QueryWrapper() {
	if (handler != nullptr) {
		const PDH_STATUS pdhStatus = PdhCloseQuery(handler);
		if (pdhStatus != ERROR_SUCCESS) {
			// WTF
		}
		handler = nullptr;
	}
}

PdhWrapper::QueryWrapper::QueryWrapper(QueryWrapper&& other) noexcept : handler(other.handler) {
	other.handler = nullptr;
}

PdhWrapper::QueryWrapper& PdhWrapper::QueryWrapper::operator=(QueryWrapper&& other) noexcept {
	if (this == &other)
		return *this;

	handler = other.handler;
	other.handler = nullptr;

	return *this;
}

PDH_HQUERY& PdhWrapper::QueryWrapper::get() {
	return handler;
}

PDH_HQUERY* PdhWrapper::QueryWrapper::getPointer() {
	return &handler;
}

bool PdhWrapper::QueryWrapper::isValid() const {
	return handler != nullptr;
}

PdhWrapper::PdhWrapper(utils::Rainmeter::Logger _log, string objectName, utils::OptionParser::OptionList counterTokens) : log(std::move(_log)) {
	QueryWrapper query;

	PDH_STATUS pdhStatus = PdhOpenQueryW(nullptr, 0, query.getPointer());
	if (pdhStatus != ERROR_SUCCESS) {
		log.error(L"PdhOpenQuery failed, status {error}", pdhStatus);
		return;
	}

	// add counters for our objectName and counterNames to the query
	// counterPath examples:
	//   counterPath = L"\\Processor(_Total)\\% Processor Time"
	//   counterPath = L"\\Physical Disk(*)\\Disk Read Bytes/Sec" (wildcard gets all instances of counterName)
	//
	// for counters with a single, unnamed instance like ObjectName=System:
	//   counterPath = L"\\System(*)\Processes" returns a single instance with an instance name of "*"
	//   counterPath = L"\\System\Processes"    returns a single instance with an instance name of ""

	counterHandlers.resize(counterTokens.size());
	string counterPath;
	for (index inx = 0; inx < index(counterHandlers.size()); ++inx) {
		counterPath = L"\\" + objectName + L"(*)" + L"\\" + string { counterTokens.get(inx) };
		pdhStatus = PdhAddEnglishCounterW(query.get(), counterPath.c_str(), 0, &counterHandlers[inx]);
		if (pdhStatus != ERROR_SUCCESS) {
			if (pdhStatus == PDH_CSTATUS_NO_OBJECT) {
				log.error(L"ObjectName '{}' does not exist", objectName);
			} else if (pdhStatus == PDH_CSTATUS_NO_COUNTER) {
				log.error(L"Counter '{}' does not exist", counterTokens.get(inx));
			} else {
				log.error(L"PdhAddEnglishCounter failed, path='{}' status {error}", counterPath, pdhStatus);
			}

			return;
		}
	}

	// add a counter to retrieve Process or Thread IDs

	string idsCounterPath;
	if ((objectName == L"Process") || (objectName == L"GPU Engine") || (objectName == L"GPU Process Memory")) {
		idsCounterPath = L"\\Process(*)\\ID Process";
		needFetchExtraIDs = true;
	} else if (objectName == L"Thread") {
		idsCounterPath = L"\\Thread(*)\\ID Thread";
		needFetchExtraIDs = true;
	} else {
		needFetchExtraIDs = false;
	}
	if (needFetchExtraIDs) {
		pdhStatus = PdhAddEnglishCounterW(query.get(), idsCounterPath.c_str(), 0, &idCounterHandler);
		if (pdhStatus != ERROR_SUCCESS) {
			log.error(L"PdhAddEnglishCounter failed, path='{}' status {error}", idsCounterPath, pdhStatus);
			return;
		}
	}

	this->query = std::move(query);
}

bool PdhWrapper::isValid() const {
	return query.isValid();
}

bool PdhWrapper::fetch(Snapshot& snapshot, Snapshot& idSnapshot) {
	idSnapshot.setCountersCount(1);
	snapshot.setCountersCount(counterHandlers.size());

	// the Pdh calls made below should not fail
	// if they do, perhaps the problem is transient and will clear itself before the next update
	// if we simply keep buffered data as-is and continue returning old values, the measure will look "stuck"
	// instead, we'll free all buffers and let data collection start over

	PDH_STATUS pdhStatus = PdhCollectQueryData(query.get());
	if (pdhStatus != ERROR_SUCCESS) {
		log.error(L"PdhCollectQueryData failed, status {error}", pdhStatus);
		return false;
	}

	// retrieve counter data for Process or Thread IDs
	if (needFetchExtraIDs) {
		DWORD bufferSize = 0;
		DWORD count = 0;
		pdhStatus = PdhGetRawCounterArrayW(idCounterHandler, &bufferSize, &count, nullptr);
		if (pdhStatus != PDH_MORE_DATA) {
			log.error(L"PdhGetRawCounterArray get dwBufferSize failed, status {error}", pdhStatus);
			return false;
		}

		idSnapshot.setBufferSize(bufferSize, count);

		PDH_RAW_COUNTER_ITEM_W* buffer = idSnapshot.getCounterPointer(0);

		pdhStatus = PdhGetRawCounterArrayW(idCounterHandler, &bufferSize, &count, buffer);
		if (pdhStatus != ERROR_SUCCESS) {
			log.error(L"PdhGetRawCounterArray failed, status {error}", pdhStatus);
			return false;
		}
	}

	DWORD bufferSize = 0;
	DWORD count = 0;
	// All counters have the same amount of elements with the same name so they need buffers of the same size
	// and we don't need to query buffer size every time.
	pdhStatus = PdhGetRawCounterArrayW(counterHandlers[0], &bufferSize, &count, nullptr);
	if (pdhStatus != PDH_MORE_DATA) {
		log.error(L"PdhGetRawCounterArray get dwBufferSize failed, status {error}", pdhStatus);
		return false;
	}

	snapshot.setBufferSize(bufferSize, count);

	// Retrieve counter data for all counters in the measure's counterList.
	for (index i = 0; i < index(counterHandlers.size()); ++i) {
		DWORD dwBufferSize2 = bufferSize;
		PDH_RAW_COUNTER_ITEM_W* buffer = snapshot.getCounterPointer(i);

		pdhStatus = PdhGetRawCounterArrayW(counterHandlers[i], &dwBufferSize2, &count, buffer);
		if (pdhStatus != ERROR_SUCCESS) {
			log.error(L"PdhGetRawCounterArray failed, status {error}", pdhStatus);
			return false;
		}
		if (dwBufferSize2 != bufferSize) {
			log.error(L"unexpected buffer size change");
			return false;
		}

	}
	itemsCount = count;

	return true;
}

index PdhWrapper::getCountersCount() const {
	return counterHandlers.size();
}

index PdhWrapper::getItemsCount() const {
	return itemsCount;
}

double PdhWrapper::extractFormattedValue(index counter, const PDH_RAW_COUNTER& current, const PDH_RAW_COUNTER& previous) const {

	// PdhCalculateCounterFromRawValue sometimes fails with the status PDH_CALC_NEGATIVE_VALUE, PDH_CALC_NEGATIVE_DENOMINATOR, or
	// PDH_CALC_NEGATIVE_TIMEBASE.  This has mainly been observed for "_Total" instances, and occurs when an instance has dropped out
	// between two query calls.  When a process ends, or when a removable drive is ejected, its counters disappear.  "_Total" thus
	// decreases between the two query calls.

	PDH_FMT_COUNTERVALUE formattedValue;
	const PDH_STATUS pdhStatus =
		PdhCalculateCounterFromRawValue(
			counterHandlers[counter],
			PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
			const_cast<PDH_RAW_COUNTER*>(&current),
			const_cast<PDH_RAW_COUNTER*>(&previous),
			&formattedValue);

	if (pdhStatus == ERROR_SUCCESS) {
		return formattedValue.doubleValue;
	}

	if (pdhStatus == PDH_CALC_NEGATIVE_VALUE || pdhStatus == PDH_CALC_NEGATIVE_DENOMINATOR || pdhStatus ==
		PDH_CALC_NEGATIVE_TIMEBASE) {
		return 0.0;
	}

	// something strange, TODO handle this
	return 0.0;
}

