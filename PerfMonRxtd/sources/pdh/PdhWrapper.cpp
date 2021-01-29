/*
 * Copyright (C) 2018-2021 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PdhWrapper.h"

#include <PdhMsg.h>

#include "option-parser/OptionList.h"

#pragma comment(lib, "pdh.lib")

using namespace perfmon::pdh;

bool PdhWrapper::init(Logger logger) {
	log = std::move(logger);

	PDH_STATUS code = PdhOpenQueryW(nullptr, 0, &query.handle);
	if (code != ERROR_SUCCESS) {
		log.error(L"PdhOpenQueryW() failed with code {}", PdhReturnCode(code));
		return false;
	}
	return true;
}

bool PdhWrapper::setCounters(sview objectName, const utils::OptionList& counterList, bool gpuExtraIds) {
	needFetchExtraIDs = gpuExtraIds;

	counterHandlers.reserve(counterList.size());
	for (auto counterOption : counterList) {
		try {
			auto counterHandle = addCounter(objectName, counterOption.asString());
			counterHandlers.emplace_back(counterHandle);
		} catch (PdhException& e) {
			switch (uint32_t(e.getCode().code)) {
			case uint32_t(PDH_CSTATUS_NO_OBJECT):
				log.error(L"ObjectName '{}' does not exist", objectName);
				break;
			case uint32_t(PDH_CSTATUS_NO_COUNTER):
				log.error(L"Counter '{}/{}' does not exist", objectName, counterOption.asString());
				break;
			default:
				log.error(L"Unknown error with counter '{}/{}': code {}, caused by {}", objectName, counterOption.asString(), e.getCode(), e.getCause());
				break;
			}
			return false;
		}
	}

	// add a counter to retrieve Process or Thread IDs
	if (objectName == L"Process") {
		try {
			auto counterHandle = addCounter(objectName, L"ID Process");
			counterHandlers.emplace_back(counterHandle);
		} catch (PdhException& e) {
			log.error(L"Can't add 'Process/ID Process' counter to query: code {}", e.getCode());
			return false;
		}
	} else if (objectName == L"Thread") {
		try {
			auto counterHandle = addCounter(objectName, L"ID Thread");
			counterHandlers.emplace_back(counterHandle);
		} catch (PdhException& e) {
			log.error(L"Can't add 'Thread/ID Thread' counter to query: code {}", e.getCode());
			return false;
		}
	} else if ((objectName == L"GPU Engine" || objectName == L"GPU Process Memory") && needFetchExtraIDs) {
		try {
			processIdCounter = addCounter(L"Process", L"ID Process");
		} catch (PdhException& e) {
			log.error(L"Can't add 'Process/ID Process' counter to query: code {}", e.getCode());
			return false;
		}
	} else { }

	return true;
}

PDH_HCOUNTER PdhWrapper::addCounter(sview objectName, sview counterName) {

	// counterPath examples:
	//   counterPath = L"\\Processor(_Total)\\% Processor Time"
	//   counterPath = L"\\Physical Disk(*)\\Disk Read Bytes/Sec" (wildcard gets all instances of counterName)
	//
	// for counters with a single, unnamed instance like ObjectName=System:
	//   counterPath = L"\\System(*)\Processes" returns a single instance with an instance name of "*"
	//   counterPath = L"\\System\Processes"    returns a single instance with an instance name of ""

	common::buffer_printer::BufferPrinter bp;
	bp.print(L"\\{}(*)\\{}", objectName, counterName);

	PDH_HCOUNTER counterHandle{};
	auto code = PdhAddEnglishCounterW(query.handle, bp.getBufferPtr(), 0, &counterHandle);
	if (code != ERROR_SUCCESS) {
		throw PdhException{ code, L"PdhAddEnglishCounterW() in QueryWrapper.addCounter()" };
	}
	return counterHandle;
}

bool PdhWrapper::fetch() {
	mainSnapshot.setCountersCount(counterHandlers.size());

	PDH_STATUS code = PdhCollectQueryData(query.handle);
	if (code != PDH_STATUS(ERROR_SUCCESS)) {
		log.error(L"PdhCollectQueryData failed, status {}", code);
		return false;
	}

	bool success = fetchSnapshot(counterHandlers, mainSnapshot);
	if (!success) {
		return false;
	}
	if (needFetchExtraIDs) {
		success = fetchSnapshot({ &processIdCounter, 1 }, processIdSnapshot);
		if (!success) {
			return false;
		}
	}

	return true;
}

double PdhWrapper::extractFormattedValue(
	index counter, const PDH_RAW_COUNTER& current,
	const PDH_RAW_COUNTER& previous
) const {

	// PdhCalculateCounterFromRawValue sometimes fails with the status PDH_CALC_NEGATIVE_VALUE, PDH_CALC_NEGATIVE_DENOMINATOR, or
	// PDH_CALC_NEGATIVE_TIMEBASE.  This has mainly been observed for "_Total" instances, and occurs when an instance has dropped out
	// between two query calls.  When a process ends, or when a removable drive is ejected, its counters disappear.  "_Total" thus
	// decreases between the two query calls.

	PDH_FMT_COUNTERVALUE formattedValue;
	const PDH_STATUS pdhStatus = PdhCalculateCounterFromRawValue(
		counterHandlers[counter],
		PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
		const_cast<PDH_RAW_COUNTER*>(&current),
		const_cast<PDH_RAW_COUNTER*>(&previous),
		&formattedValue
	);

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

bool PdhWrapper::fetchSnapshot(array_span<PDH_HCOUNTER> counters, PdhSnapshot& snapshot) {
	snapshot.setCountersCount(counters.size());

	DWORD bufferSize = 0;
	DWORD count = 0;
	// call to PdhGetRawCounterArrayW with bufferSize==0 should return with PDH_MORE_DATA and bufferSize and count set to appropriate value
	// Since all counters in this function are from one object, they all have the same amount of the same items
	// so buffer size should also be the same for all counters
	PDH_STATUS code = PdhGetRawCounterArrayW(counters[0], &bufferSize, &count, nullptr);
	if (code != PDH_STATUS(PDH_MORE_DATA)) {
		log.error(L"PdhGetRawCounterArray(size=0) failed, status {}", PdhReturnCode(code));
		return false;
	}

	snapshot.setBufferSize(bufferSize, count);

	// Retrieve counter data for all counters in the measure's counterList.
	for (index i = 0; i < index(counters.size()); ++i) {
		DWORD dwBufferSize2 = bufferSize;
		code = PdhGetRawCounterArrayW(counters[i], &dwBufferSize2, &count, snapshot.getCounterPointer(i));
		if (code != PDH_STATUS(ERROR_SUCCESS)) {
			log.error(L"PdhGetRawCounterArray failed, status {}", PdhReturnCode(code));
			return false;
		}
		if (dwBufferSize2 != bufferSize) {
			log.error(L"unexpected buffer size change");
			return false;
		}
	}

	return true;
}
