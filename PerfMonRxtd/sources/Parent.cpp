/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#define _CRT_SECURE_NO_WARNINGS

#include "RainmeterAPI.h"

#include <pdh.h>
#include <PdhMsg.h>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <functional>
#include <unordered_map>
#include <string>
#include <string_view>

#include "expressions.h"
#include "PerfMonRXTD.h"
#include "Parent.h"
#include "utils.h"

namespace pmr {
	std::vector<ParentData*> parentMeasuresVector;
}

pmr::ParentData::~ParentData() {
	if (hQuery != nullptr) {
		PDH_STATUS pdhStatus = PdhCloseQuery(hQuery);
		if (pdhStatus != ERROR_SUCCESS) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhCloseQuery failed, status=0x%x", pdhStatus);
		}
		hQuery = nullptr;
	}
	freeBuffers();
	const auto iter = std::find(parentMeasuresVector.begin(), parentMeasuresVector.end(), this);
	parentMeasuresVector.erase(iter);
}
pmr::ParentData::ParentData(TypeHolder* typeHolder)
	: typeHolder(typeHolder) {
	parentMeasuresVector.push_back(this);

	objectName = RmReadString(typeHolder->rm, L"ObjectName", L"");
	const std::wstring counterList = RmReadString(typeHolder->rm, L"CounterList", L"");
	instanceIndexOffset = RmReadInt(typeHolder->rm, L"InstanceIndexOffset", 0);

	std::vector<std::wstring> counterTokens = Tokenize(counterList, L"|");
	hCounter.resize(counterTokens.size());

	// validate options

	if (objectName == L"") {
		RmLogF(typeHolder->rm, LOG_ERROR, L"ObjectName must be specified");
		typeHolder->broken = true;
		return;
	}
	if (hCounter.empty()) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"CounterList must have at least one entry");
		typeHolder->broken = true;
		return;
	}


	PDH_STATUS pdhStatus = PdhOpenQueryW(nullptr, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"PdhOpenQuery failed, status=0x%x", pdhStatus);
		typeHolder->broken = true;
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

	for (unsigned int inx = 0; inx < hCounter.size(); ++inx) {
		std::wstring counterPath = L"\\" + objectName + L"(*)" + L"\\" + counterTokens[inx];
		pdhStatus = PdhAddEnglishCounterW(hQuery, counterPath.c_str(), 0, &hCounter[inx]);
		if (pdhStatus != ERROR_SUCCESS) {
			if (pdhStatus == PDH_CSTATUS_NO_OBJECT)
				RmLogF(typeHolder->rm, LOG_ERROR, L"ObjectName '%s' does not exist", objectName.c_str());
			else if (pdhStatus == PDH_CSTATUS_NO_COUNTER)
				RmLogF(typeHolder->rm, LOG_ERROR, L"Counter '%s' does not exist", counterTokens[inx].c_str());
			else
				RmLogF(typeHolder->rm, LOG_ERROR, L"PdhAddEnglishCounter failed, path='%s' status=0x%x", counterPath.c_str(), pdhStatus);

			PdhCloseQuery(hQuery);
			hQuery = nullptr;
			typeHolder->broken = true;
			return;
		}
	}

	// add a counter to retrieve Process or Thread IDs

	std::wstring idsCounterPath;
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
		pdhStatus = PdhAddEnglishCounterW(hQuery, idsCounterPath.c_str(), 0, &hCounterID);
		if (pdhStatus != ERROR_SUCCESS) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhAddEnglishCounter failed, path='%s' status=0x%x", idsCounterPath.c_str(), pdhStatus);
			PdhCloseQuery(hQuery);
			hQuery = nullptr;
			typeHolder->broken = true;
			return;
		}
	}

	rawBuffersCurrent.setBuffersCount(hCounter.size());
	rawBuffersPrevious.setBuffersCount(hCounter.size());

	typeHolder->resultString = resultString;
}
void pmr::ParentData::reload() {
	if (typeHolder->broken) {
		typeHolder->temporaryBroken = true;
		return;
	}
	typeHolder->temporaryBroken = false;

	needUpdate = true;

	sortIndex = RmReadInt(typeHolder->rm, L"SortIndex", 0);
	rollup = RmReadInt(typeHolder->rm, L"Rollup", 0) != 0;
	syncRawFormatted = RmReadInt(typeHolder->rm, L"SyncRawFormatted", 1) != 0;
	limitIndexOffset = RmReadInt(typeHolder->rm, L"LimitIndexOffset", 0) != 0;
	if (limitIndexOffset && instanceIndexOffset < 0) {
		instanceIndexOffset = 0;
	}
	keepDiscarded = RmReadInt(typeHolder->rm, L"KeepDiscarded", 0) != 0;

	LPCWSTR str = RmReadString(typeHolder->rm, L"SortBy", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"None") == 0)
		sortBy = SortBy::NONE;
	else if (_wcsicmp(str, L"InstanceName") == 0)
		sortBy = SortBy::INSTANCE_NAME;
	else if (_wcsicmp(str, L"RawCounter") == 0)
		sortBy = SortBy::RAW_COUNTER;
	else if (_wcsicmp(str, L"FormattedCounter") == 0)
		sortBy = SortBy::FORMATTED_COUNTER;
	else if (_wcsicmp(str, L"Expression") == 0)
		sortBy = SortBy::EXPRESSION;
	else if (_wcsicmp(str, L"RollupExpression") == 0)
		sortBy = SortBy::ROLLUP_EXPRESSION;
	else if (_wcsicmp(str, L"Count") == 0)
		sortBy = SortBy::COUNT;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortBy '%s' is invalid, set to 'None'", str);
		sortBy = SortBy::NONE;
	}

	str = RmReadString(typeHolder->rm, L"SortOrder", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Descending") == 0)
		sortOrder = SortOrder::DESCENDING;
	else if (_wcsicmp(str, L"Ascending") == 0)
		sortOrder = SortOrder::ASCENDING;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortOrder '%s' is invalid, set to 'Descending'", str);
		sortOrder = SortOrder::DESCENDING;
	}

	str = RmReadString(typeHolder->rm, L"SortRollupFunction", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Sum") == 0)
		sortRollupFunction = pmre::RollupFunction::SUM;
	else if (_wcsicmp(str, L"Average") == 0)
		sortRollupFunction = pmre::RollupFunction::AVERAGE;
	else if (_wcsicmp(str, L"Minimum") == 0)
		sortRollupFunction = pmre::RollupFunction::MINIMUM;
	else if (_wcsicmp(str, L"Maximum") == 0)
		sortRollupFunction = pmre::RollupFunction::MAXIMUM;
	else if (_wcsicmp(str, L"Count") == 0) {
		RmLogF(typeHolder->rm, LOG_WARNING, L"SortRollupFunction 'Count' is deprecated, SortBy set to 'Count'", str);
		sortBy = SortBy::COUNT;
		sortRollupFunction = pmre::RollupFunction::SUM;
	} else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortRollupFunction '%s' is invalid, set to 'Sum'", str);
		sortRollupFunction = pmre::RollupFunction::SUM;
	}

	parseList(blacklist, RmReadString(typeHolder->rm, L"Blacklist", L""), true);
	parseList(blacklistOrig, RmReadString(typeHolder->rm, L"BlacklistOrig", L""), false);
	parseList(whitelist, RmReadString(typeHolder->rm, L"Whitelist", L""), true);
	parseList(whitelistOrig, RmReadString(typeHolder->rm, L"WhitelistOrig", L""), false);

	std::vector<std::wstring> expressionTokens = Tokenize(RmReadString(typeHolder->rm, L"ExpressionList", L""), L"|");
	expressions.reserve(expressionTokens.size());
	for (int i = 0; static_cast<std::vector<std::wstring>::size_type>(i) < expressionTokens.size(); ++i) {
		pmrexp::Parser parser(expressionTokens[i]);
		parser.parse();
		if (parser.isError()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Expression %d can't be parsed: %s", i, expressionTokens[i].c_str());
			pmrexp::ExpressionTreeNode node;
			node.type = pmrexp::ExpressionType::NUMBER;
			node.number = 0;
			expressions.emplace_back(node);
			continue;
		}
		pmrexp::ExpressionTreeNode expression = parser.getExpression();
		expression.simplify();
		const int maxRef = expression.maxExpRef();
		if (maxRef >= i) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Expression %d can't reference Expression %d", i, maxRef);
			pmrexp::ExpressionTreeNode node;
			node.type = pmrexp::ExpressionType::NUMBER;
			node.number = 0;
			expressions.emplace_back(node);
			continue;
		}
		const int maxRURef = expression.maxExpRef();
		if (maxRURef >= 0) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Expressions can't reference RollupExpressions");
			pmrexp::ExpressionTreeNode node;
			node.type = pmrexp::ExpressionType::NUMBER;
			node.number = 0;
			expressions.emplace_back(node);
			continue;
		}
		expression.processRefs([](pmrexp::reference& ref) {
			if (!ref.useOrigName) {
				CharUpperW(&ref.name[0]);
			}
		});
		expressions.emplace_back(expression);
	}

	std::vector<std::wstring> rollupExpressionTokens = Tokenize(RmReadString(typeHolder->rm, L"RollupExpressionList", L""), L"|");
	rollupExpressions.reserve(rollupExpressionTokens.size());
	for (int i = 0; static_cast<std::vector<std::wstring>::size_type>(i) < rollupExpressionTokens.size(); ++i) {
		pmrexp::Parser parser(rollupExpressionTokens[i]);
		parser.parse();
		if (parser.isError()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"RollupExpression %d can't be parsed", i);
			pmrexp::ExpressionTreeNode node;
			node.type = pmrexp::ExpressionType::NUMBER;
			node.number = 0;
			rollupExpressions.emplace_back(node);
			continue;
		}
		pmrexp::ExpressionTreeNode expression = parser.getExpression();
		expression.simplify();
		const int maxRef = expression.maxRUERef();
		if (maxRef >= i) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"RollupExpression %d can't reference RollupExpression %d", i, maxRef);
			pmrexp::ExpressionTreeNode node;
			node.type = pmrexp::ExpressionType::NUMBER;
			node.number = 0;
			rollupExpressions.emplace_back(node);
			continue;
		}
		expression.processRefs([](pmrexp::reference& ref) {
			if (!ref.useOrigName) {
				CharUpperW(&ref.name[0]);
			}
		});
		rollupExpressions.emplace_back(expression);
	}

	if ((sortBy == SortBy::RAW_COUNTER || sortBy == SortBy::FORMATTED_COUNTER)
		&& !indexIsInBounds(sortIndex, 0, static_cast<int>(hCounter.size() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortIndex must be between 0 and %d, set to 0", hCounter.size() - 1);
		sortIndex = 0;
	} else if (sortBy == SortBy::EXPRESSION) {
		if (expressions.empty()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Sort by Expression requires at least 1 Expression specified. Set to None.");
			sortBy = SortBy::NONE;
		} else if (!indexIsInBounds(sortIndex, 0, static_cast<int>(expressions.size() - 1))) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"SortIndex for Expression must be between 0 and %d, set to 0", expressions.size());
			sortIndex = 0;
		}
	} else if (sortBy == SortBy::ROLLUP_EXPRESSION) {
		if (!rollup) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"RollupExpressions can't be used for sort if rollup is disabled. Set to None.");
			sortBy = SortBy::NONE;
		} else if (rollupExpressions.empty()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Sort by RollupExpression requires at least 1 RollupExpression specified. Set to None.");
			sortBy = SortBy::NONE;
		} else if (!indexIsInBounds(sortIndex, 0, static_cast<int>(rollupExpressions.size() - 1))) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"SortIndex for RollupExpression must be between 0 and %d, set to 0", rollupExpressions.size());
			sortIndex = 0;
		}
	}


	if ((objectName == L"GPU Engine") || (objectName == L"GPU Process Memory")) {

		const wchar_t* displayName = RmReadString(typeHolder->rm, L"DisplayName", L"");;
		if (_wcsicmp(displayName, L"") == 0 || _wcsicmp(displayName, L"Original") == 0)
			generateNamesFunction = &ParentData::modifyNameNone;
		else if (_wcsicmp(displayName, L"ProcessName") == 0 || _wcsicmp(displayName, L"GpuProcessName") == 0)
			generateNamesFunction = &ParentData::modifyNameGPUProcessName;
		else if (_wcsicmp(displayName, L"EngType") == 0 || _wcsicmp(displayName, L"GpuEngType") == 0)
			generateNamesFunction = &ParentData::modifyNameGPUEngtype;
		else {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Object '%s' don't support DisplayName '%s', set to 'Original'", objectName.c_str(), displayName);
			generateNamesFunction = &ParentData::modifyNameNone;
		}

	} else if (objectName == L"LogicalDisk") {

		const wchar_t* displayName = RmReadString(typeHolder->rm, L"DisplayName", L"");;
		if (_wcsicmp(displayName, L"") == 0 || _wcsicmp(displayName, L"Original") == 0)
			generateNamesFunction = &ParentData::modifyNameNone;
		else if (_wcsicmp(displayName, L"DriveLetter") == 0)
			generateNamesFunction = &ParentData::modifyNameLogicalDiskDriveLetter;
		else if (_wcsicmp(displayName, L"MountFolder") == 0)
			generateNamesFunction = &ParentData::modifyNameLogicalDiskMountPath;
		else {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Object '%s' don't support DisplayName '%s', set to 'Original'", objectName.c_str(), displayName);
			generateNamesFunction = &ParentData::modifyNameNone;
		}

	} else if (objectName == L"Process") {
		generateNamesFunction = &ParentData::modifyNameProcess;
	} else if (objectName == L"Thread") {
		generateNamesFunction = &ParentData::modifyNameThread;
	} else {
		generateNamesFunction = &ParentData::modifyNameNone;
	}

	//RmLogF(typeHolder->rm, LOG_DEBUG, L"reload parent complete");
}

bool pmr::ParentData::canGetRaw() const {
	return !rawBuffersCurrent.isEmpty() && (!syncRawFormatted || !rawBuffersPrevious.isEmpty());
}
bool pmr::ParentData::canGetFormatted() const {
	return !rawBuffersCurrent.isEmpty() && !rawBuffersPrevious.isEmpty();
}

const pmr::instanceKeyItem* pmr::ParentData::findInstance(const pmrexp::reference& ref, unsigned long sortedIndex) const {
	if (!ref.named) {
		// get instance by index
		const std::vector<instanceKeyItem>& instances = rollup ? vectorRollupKeys : vectorInstanceKeys;
		sortedIndex += instanceIndexOffset;
		if ((sortedIndex < 0) || (sortedIndex >= instances.size()))
			return nullptr;
		return &instances[sortedIndex];
	}
	// get instance by name
	return findInstanceByName(ref, rollup);
}

const pmr::instanceKeyItem* pmr::ParentData::findInstanceByNameInList(const pmrexp::reference& ref, const std::vector<instanceKeyItem>& instances) const {
	if (ref.useOrigName) {
		for (const auto& item : instances) {
			if (stringsMatch(namesCurrent[item.originalIndexes.originalCurrentInx].originalName, ref.name.c_str(), ref.namePartialMatch)) {
				return &item;
			}
		}
	} else {
		for (const auto& item : instances) {
			if (stringsMatch(item.sortName, ref.name.c_str(), ref.namePartialMatch)) {
				return &item;
			}
		}
	}
	return nullptr;
}

const pmr::instanceKeyItem* pmr::ParentData::findInstanceByName(const pmrexp::reference& ref, bool useRollup) const {
	if (ref.discarded) {
		return findInstanceByNameInList(ref, vectorDiscardedKeys);
	}
	if (useRollup) {
		return findInstanceByNameInList(ref, vectorRollupKeys);
	} else {
		return findInstanceByNameInList(ref, vectorInstanceKeys);
	}
}

const wchar_t* pmr::ParentData::getName(const instanceKeyItem& instance, pmre::ResultString stringType) const {
	if (stringType == pmre::ResultString::NUMBER) {
		return L"";
	}
	if (instance.vectorIndexes.empty()) { // rollup instances have vector indexes filled and usual instances don't have
		const modifiedNameItem& name = namesCurrent[instance.originalIndexes.originalCurrentInx];
		switch (stringType) {
		case pmre::ResultString::ORIGINAL_NAME:
			return  name.originalName;
		case pmre::ResultString::UNIQUE_NAME:
			return  name.uniqueName;
		case pmre::ResultString::DISPLAY_NAME:
			return  name.displayName;
		default:
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected result string type %d", stringType);
			return L"";
		}
	} else {
		const modifiedNameItem& name = namesCurrent[instance.vectorIndexes[0].originalCurrentInx];
		switch (stringType) {
		case pmre::ResultString::ORIGINAL_NAME:
		case pmre::ResultString::UNIQUE_NAME:
		case pmre::ResultString::DISPLAY_NAME:
			return  name.displayName;
		default:
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected result string type %d", stringType);
			return L"";
		}
	}
}
void pmr::ParentData::freeBuffers() {
	rawBuffersCurrent.erase();
	rawBuffersPrevious.erase();
	idRawBuffer.erase();

	namesCurrent.erase();
	namesBufferCurrent.erase();
	namesPrevious.erase();
	namesBufferPrevious.erase();
}

bool pmr::ParentData::nameIsInList(const std::vector<std::pair<std::wstring, bool>>& list, const wchar_t* name) {
	for (auto& pair : list) {
		if (stringsMatch(name, pair.first.c_str(), pair.second)) {
			return true;
		}
	}
	return false;
}
void pmr::ParentData::parseList(std::vector<std::pair<std::wstring, bool>>& list, const std::wstring& listString, bool upperCase) {
	std::vector<std::wstring> tokens = Tokenize(listString, L"|");
	list.reserve(tokens.size());
	for (auto& token : tokens) {
		const auto len = token.size();
		if (upperCase) {
			CharUpperW(&token[0]);
		}
		if (len >= 2 && token[0] == L'*' && token[len - 1] == L'*') {
			list.emplace_back(token.substr(1, len - 2), true);
		} else {
			list.emplace_back(token, false);
		}
	}
}
bool pmr::ParentData::isInstanceAllowed(const wchar_t* searchName, const wchar_t* originalName) const {
	// if names are not in any whitelist which is not empty — instance is not allowed
	if (!whitelist.empty() && !whitelistOrig.empty() && !(nameIsInList(whitelist, searchName) || nameIsInList(whitelistOrig, originalName)))
		return false;
	if (!whitelist.empty() && whitelistOrig.empty() && !nameIsInList(whitelist, searchName))
		return false;
	if (whitelist.empty() && !whitelistOrig.empty() && !nameIsInList(whitelistOrig, originalName))
		return false;
	// but if names are in any blacklist — instance is not allowed despite being whitelisted
	if (nameIsInList(blacklist, searchName) || nameIsInList(blacklistOrig, originalName))
		return false;

	return true;
}

bool pmr::ParentData::fetchNewData() {
	// the Pdh calls made below should not fail
	// if they do, perhaps the problem is transient and will clear itself before the next update
	// if we simply keep buffered data as-is and continue returning old values, the measure will look "stuck"
	// instead, we'll free all buffers and let data collection start over

	PDH_STATUS pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"PdhCollectQueryData failed, status=0x%x", pdhStatus);
		freeBuffers();
		return false;
	}

	// retrieve counter data for Process or Thread IDs
	if (needFetchExtraIDs) {
		DWORD bufferSize = 0;
		DWORD itemCount = 0;
		pdhStatus = PdhGetRawCounterArrayW(hCounterID, &bufferSize, &itemCount, nullptr);
		if (pdhStatus != PDH_MORE_DATA) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhGetRawCounterArray get dwBufferSize failed, status=0x%x", pdhStatus);
			freeBuffers();
			return false;
		}
		const bool success = idRawBuffer.ensureSize(bufferSize);
		if (!success) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"idRawBuffer.setBufferSize() failed, %ul", bufferSize);
			freeBuffers();
			return false;
		}
		PDH_RAW_COUNTER_ITEM_W* buffer = idRawBuffer.getPointer();

		pdhStatus = PdhGetRawCounterArrayW(hCounterID, &bufferSize, &itemCount, buffer);
		if (pdhStatus != ERROR_SUCCESS) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhGetRawCounterArray failed, status=0x%x", pdhStatus);
			freeBuffers();
			return false;
		}
	}

	DWORD bufferSize = 0;
	DWORD itemCount = 0;
	// All counters have the same amount of elements with the same name so they need buffers of the same size
	// and we don't need to query buffer size every time.
	pdhStatus = PdhGetRawCounterArrayW(hCounter[0], &bufferSize, &itemCount, nullptr);
	if (pdhStatus != PDH_MORE_DATA) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"PdhGetRawCounterArray get dwBufferSize failed, status=0x%x", pdhStatus);
		freeBuffers();
		return false;
	}
	const bool success = rawBuffersCurrent.setBufferSize(bufferSize);
	if (!success) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"rawBuffersCurrent.setBufferSize() failed, %ul", bufferSize);
		freeBuffers();
		return false;
	}
	// Retrieve counter data for all counters in the measure's counterList.
	for (unsigned int i = 0; i < hCounter.size(); ++i) {
		DWORD dwBufferSize2 = bufferSize;
		PDH_RAW_COUNTER_ITEM_W* buffer = rawBuffersCurrent.getBuffer(i);

		pdhStatus = PdhGetRawCounterArrayW(hCounter[i], &dwBufferSize2, &itemCount, buffer);
		if (pdhStatus != ERROR_SUCCESS) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhGetRawCounterArray failed, status=0x%x", pdhStatus);
			freeBuffers();
			return false;
		}
		if (dwBufferSize2 != bufferSize) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected buffer size change", pdhStatus);
			freeBuffers();
			return false;
		}

	}
	itemCountCurrent = itemCount;
	createModifiedNames();

	return true;
}
void pmr::ParentData::update() {

	if (!stopped) {
		// Let's keep all buffers to minimize reallocations. 
		// We only swap current and previous values so that we always have two snapshots of data
		std::swap(rawBuffersCurrent, rawBuffersPrevious);
		std::swap(itemCountCurrent, itemCountPrevious);

		std::swap(namesCurrent, namesPrevious);
		std::swap(namesBufferCurrent, namesBufferPrevious);

		const bool res = fetchNewData();
		if (!res) {
			return;
		}
	}

	typeHolder->resultDouble = itemCountCurrent;
	if (!canGetRaw()) {
		wcscpy_s(resultString, L"no data");
		return;
	}

	// needUpdate indicates that measure was reloaded and blacklists or sorting options could have changed
	if (!stopped || needUpdate) {
		needUpdate = false;

		foundNamesInstances.clear();
		foundNamesRollup.clear();
		totalsCache.clear();

		if (rawBuffersPrevious.isEmpty()) {
			buildInstanceKeysZero();
		} else {
			buildInstanceKeys();
		}
		if (rollup) {
			buildRollupKeys();
		}
		sortInstanceKeys(rollup);
	}

	std::wstring status = L"count=" + std::to_wstring(itemCountCurrent) + L"/" +
		std::to_wstring(vectorInstanceKeys.size()) + L"/" +
		std::to_wstring(vectorRollupKeys.size()) +
		L" sortby=" + std::to_wstring(static_cast<unsigned>(sortBy)) +
		L" rollup=" + (rollup ? L"1" : L"0") +
		L" stopped= " + (stopped ? L"1" : L"0");

	wcscpy_s(resultString, status.c_str());
}

void pmr::ParentData::setStopped(const bool value) {
	stopped = value;
}
void pmr::ParentData::changeStopState() {
	stopped = !stopped;
}
void pmr::ParentData::setIndexOffset(const int value) {
	if (value < 0) {
		instanceIndexOffset = 0;
	} else {
		instanceIndexOffset = value;
	}
}
int pmr::ParentData::getIndexOffset() const {
	return instanceIndexOffset;
}

void pmr::ParentData::sortInstanceKeys(const bool rollup) {
	std::vector<instanceKeyItem>& instances = rollup ? vectorRollupKeys : vectorInstanceKeys;
	if (sortBy == SortBy::NONE || instances.empty()) {
		return;
	}
	if (sortBy == SortBy::INSTANCE_NAME) {
		switch (sortOrder) {
		case SortOrder::ASCENDING:
			std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return wcscmp(lhs.sortName, rhs.sortName) > 0; });
			break;
		case SortOrder::DESCENDING:
			std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return wcscmp(lhs.sortName, rhs.sortName) < 0; });
			break;
		default:
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected sortOrder %d", sortOrder);
			break;
		}
		return;
	}
	switch (sortBy) {
	case SortBy::RAW_COUNTER:
	{
		if (rollup) {
			for (auto& instance : instances) {
				// instance.sortValue = static_cast<double>(calculateRawRollup(sortIndex, instance, sortRollupFunction));
				instance.sortValue = calculateRollup<&ParentData::calculateRaw>(sortRollupFunction, sortIndex, instance);
			}
		} else {
			PDH_RAW_COUNTER_ITEM_W* lpRawBufferCurrent = rawBuffersCurrent.getBuffer(sortIndex);
			for (auto& instance : instances) {
				instance.sortValue = static_cast<double>(lpRawBufferCurrent[instance.originalIndexes.originalCurrentInx].RawValue.FirstValue);
			}
		}
		break;
	}
	case SortBy::FORMATTED_COUNTER:
	{
		if (!canGetFormatted()) {
			for (auto& instance : instances) {
				instance.sortValue = 0.0;
			}
			return;
		}
		if (rollup) {
			for (auto& instance : instances) {
				instance.sortValue = calculateRollup<&ParentData::calculateFormatted>(sortRollupFunction, sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = calculateFormatted(sortIndex, instance.originalIndexes);
			}
		}
		break;
	}
	case SortBy::EXPRESSION:
	{
		if (rollup) {
			const pmrexp::ExpressionTreeNode& expression = expressions[sortIndex];
			for (auto& instance : instances) {
				expCurrentItem = &instance;
				instance.sortValue = calculateExpressionRollup(expression, sortRollupFunction);
			}
		} else {
			const pmrexp::ExpressionTreeNode& expression = expressions[sortIndex];
			for (auto& instance : instances) {
				expCurrentItem = &instance;
				instance.sortValue = calculateExpression<&ParentData::resolveReference>(expression);
			}
		}
		break;
	}
	case SortBy::ROLLUP_EXPRESSION:
	{
		if (!rollup) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"resolving RollupExpression without rollup", sortBy);
			return;
		}
		const pmrexp::ExpressionTreeNode& expression = rollupExpressions[sortIndex];
		for (auto& instance : instances) {
			expCurrentItem = &instance;
			instance.sortValue = calculateExpression<&ParentData::resolveRollupReference>(expression);
		}
		break;
	}
	case SortBy::COUNT:
	{
		if (!rollup) {
			return;
		}
		for (auto& instance : instances) {
			instance.sortValue = static_cast<double>(instance.vectorIndexes.size());
		}
		break;
	}
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected sortBy %d", sortBy);
		return;
	}

	switch (sortOrder) {
	case SortOrder::ASCENDING:
		std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return lhs.sortValue < rhs.sortValue; });
		break;
	case SortOrder::DESCENDING:
		std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return lhs.sortValue > rhs.sortValue; });
		break;
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected sortOrder %d", sortOrder);
		break;
	}
}
void pmr::ParentData::buildInstanceKeysZero() {
	// when finished, the number of items in vectorInstanceKeys may be less than itemCountCurrent because:
	//   some may have been filtered out by blacklist/whitelist
	//   some may not exist in both the current and previous buffers
	//   some may have had PdhCalculateCounterFromRawValue fail

	vectorInstanceKeys.clear();
	vectorInstanceKeys.reserve(itemCountCurrent);
	vectorDiscardedKeys.clear();

	instanceKeyItem instanceKey;
	for (decltype(itemCountCurrent) currentInx = 0; currentInx < itemCountCurrent; ++currentInx) {
		const modifiedNameItem& item = namesCurrent[currentInx];

		instanceKey.sortName = item.searchName;
		instanceKey.originalIndexes.originalCurrentInx = currentInx;
		instanceKey.originalIndexes.originalPreviousInx = 0;

		if (isInstanceAllowed(item.searchName, item.originalName)) {
			vectorInstanceKeys.push_back(instanceKey);
		} else if (keepDiscarded) {
			vectorDiscardedKeys.push_back(instanceKey);
		}
	}
}
void pmr::ParentData::buildInstanceKeys() {
	// when finished, the number of items in vectorInstanceKeys may be less than itemCountCurrent because:
	//   some may have been filtered out by blacklist/whitelist
	//   some may not exist in both the current and previous buffers
	//   some may have had PdhCalculateCounterFromRawValue fail

	vectorInstanceKeys.clear();
	vectorInstanceKeys.reserve(itemCountCurrent);
	vectorDiscardedKeys.clear();

	instanceKeyItem instanceKey;
	for (decltype(itemCountCurrent) currentInx = 0; currentInx < itemCountCurrent; ++currentInx) {
		const modifiedNameItem& item = namesCurrent[currentInx];

		// the target instance must have both current and previous values
		const int previousInx = findPreviousName(currentInx);
		if (previousInx < 0)
			continue;

		instanceKey.sortName = item.searchName;
		instanceKey.originalIndexes.originalCurrentInx = currentInx;
		instanceKey.originalIndexes.originalPreviousInx = previousInx;

		if (isInstanceAllowed(item.searchName, item.originalName)) {
			vectorInstanceKeys.push_back(instanceKey);
		} else if (keepDiscarded) {
			vectorDiscardedKeys.push_back(instanceKey);
		}
	}
}
void pmr::ParentData::buildRollupKeys() {
	// I tried to sort vectorInstanceKeys and then rollup instances with identical names but it seems to be slower than using maps
	// std::unordered_map seems to be slightly faster than std::map
	std::unordered_map<std::wstring_view, instanceKeyItem> mapRollupKeys(vectorInstanceKeys.size());

	for (const auto& instance : vectorInstanceKeys) {
		const indexesItem& indexes = instance.originalIndexes;

		instanceKeyItem& item = mapRollupKeys[instance.sortName];
		if (item.sortName == nullptr) { // this item has just been created
			item.sortName = instance.sortName;
			item.originalIndexes = indexes;
		} else { // else this item already exists in the map
			item.vectorIndexes.push_back(indexes);
		}
	}

	// copy rollupKeys to the measure's rollup vector because vectors can be sorted, but maps cannot
	vectorRollupKeys.clear();
	vectorRollupKeys.reserve(mapRollupKeys.size());
	for (auto& mapRollupKey : mapRollupKeys) {
		vectorRollupKeys.emplace_back(mapRollupKey.second);
	}
}

double pmr::ParentData::getValue(const pmrexp::reference& ref, const instanceKeyItem* instance, void* rm) const {
	switch (ref.type) {
	case pmrexp::ReferenceType::COUNTER_RAW:
		if (!indexIsInBounds(ref.counter, 0, static_cast<int>(hCounter.size() - 1))) {
			RmLogF(rm, LOG_ERROR, L"Trying to get a non-existing counter %d.", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::RAW_COUNTER, ref.counter, ref.rollupFunction);
		}
		if (instance == nullptr) {
			return 0.0;
		}
		if (rollup) {
			return calculateRollup<&ParentData::calculateRaw>(ref.rollupFunction, ref.counter, *instance);
		}
		return static_cast<double>(calculateRaw(ref.counter, instance->originalIndexes));

	case pmrexp::ReferenceType::COUNTER_FORMATTED:
		if (!indexIsInBounds(ref.counter, 0, static_cast<int>(hCounter.size() - 1))) {
			RmLogF(rm, LOG_ERROR, L"Trying to get a non-existing counter %d.", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::FORMATTED_COUNTER, ref.counter, ref.rollupFunction);
		}
		if (instance == nullptr) {
			return 0.0;
		}
		if (rollup) {
			return calculateRollup<&ParentData::calculateFormatted>(ref.rollupFunction, ref.counter, *instance);
		}
		return calculateFormatted(ref.counter, instance->originalIndexes);

	case pmrexp::ReferenceType::EXPRESSION:
		if (!indexIsInBounds(ref.counter, 0, static_cast<int>(expressions.size() - 1))) {
			RmLogF(rm, LOG_ERROR, L"Trying to get a non-existing expression %d.", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::EXPRESSION, ref.counter, ref.rollupFunction);
		}
		if (instance == nullptr) {
			return 0.0;
		}
		expCurrentItem = instance;
		if (rollup) {
			return calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		}
		return calculateExpression<&ParentData::resolveReference>(expressions[ref.counter]);

	case pmrexp::ReferenceType::ROLLUP_EXPRESSION:
		if (!indexIsInBounds(ref.counter, 0, static_cast<int>(rollupExpressions.size() - 1))) {
			RmLogF(rm, LOG_ERROR, L"Trying to get a non-existing expression %d.", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::ROLLUP_EXPRESSION, ref.counter, ref.rollupFunction);
		}
		if (rollup) {
			if (instance == nullptr) {
				return 0.0;
			}
			expCurrentItem = instance;
			return calculateExpression<&ParentData::resolveRollupReference>(rollupExpressions[ref.counter]);
		}
		RmLogF(rm, LOG_ERROR, L"RollupExpression can't be evaluated without rollup");
		return 0.0;
	case pmrexp::ReferenceType::COUNT:
		if (ref.total) {
			if (rollup) {
				return calculateRollupCountTotal(ref.rollupFunction);
			} else {
				return calculateCountTotal(ref.rollupFunction);
			}
		} else {
			if (instance == nullptr) {
				return 0.0;
			}
			if (rollup) {
				return static_cast<double>(instance->vectorIndexes.size());
			} else {
				return 1.0;
			}
		}
	case pmrexp::ReferenceType::UNKNOWN:
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected refType in getValue(): %d.", ref.type);
		return 0.0;
	}
}
long long pmr::ParentData::copyUpperCase(const wchar_t* source, wchar_t* dest) {
	wchar_t* const start = dest;
	while (true) {
		const wchar_t c = *source;
		*dest = c;
		dest++;
		source++;
		if (c == L'\0') {
			break;
		}
	}
	CharUpperW(start);
	return dest - start;
}

void pmr::ParentData::modifyNameNone() {
	namesCurrent.ensureElementsCount(itemCountCurrent);

	const size_t originalNamesSize = rawBuffersCurrent.getBufferSize() - sizeof(PDH_RAW_COUNTER_ITEM_W) * itemCountCurrent;
	namesBufferCurrent.ensureSize(originalNamesSize);
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		const wchar_t* namePtr = rawBuffersCurrent[0][instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;
		item.uniqueName = namePtr;
		item.displayName = namePtr;

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::modifyNameProcess() {
	// when using PDH, objectName "Process" returns instance names of the form "<processname>"
	// these names are non-unique: there can be multiple "csrss", "svchost", "explorer", ... processes
	//
	// the counter "ID Process" provides an ID number that is unique across all processes on the system
	// PIDs are reused, but we'll assume they are unique over short periods of time (two update cycles)
	// the "_Total" and "Idle" instance names have PIDs of 0
	//
	// we need unique names to match processes between succesive counter data queries
	// simply replacing the instance name with the PID would be (mostly) sufficient, but not very human friendly
	// this routine adds "#pid" to make the names unique
	//
	// Unique names will be longer than original names since "#pid" will be appended.
	// We'll reserve 10 extra symbols for each name to be safe.

	// Instances in the main buffers and id buffer are synchronized, so we can use id buffer alone.
	PDH_RAW_COUNTER_ITEM_W * lpRawBufferIDCurrent = idRawBuffer.getPointer();

	namesCurrent.ensureElementsCount(itemCountCurrent);

	const size_t originalNamesSize = rawBuffersCurrent.getBufferSize() - sizeof(PDH_RAW_COUNTER_ITEM_W) * itemCountCurrent;
	namesBufferCurrent.ensureSize(originalNamesSize * 2 + static_cast<size_t>(itemCountCurrent) * 10 * sizeof(wchar_t));
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		const wchar_t* namePtr = lpRawBufferIDCurrent[instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;
		item.displayName = namePtr;

		// unique name is originalName#pid
		item.uniqueName = namesBuffer;
		const long long pid = lpRawBufferIDCurrent[instanceInx].RawValue.FirstValue;
		const int length = swprintf(namesBuffer, originalNamesSize, L"%s#%I64u", namePtr, pid);
		namesBuffer += length + 1;

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::modifyNameThread() {
	// when using PDH, objectName "Thread" returns instance names of the form "<processname>/<threadinstance>"
	// these names are non-unique: there can be multiple "csrss/0", "svchost/2", "explorer/1", ... each from a different process
	// worse, the thread instance numbers ("/0", "/1", ...) for a given process are resused as threads come and go
	//
	// the counter "ID Thread" provides an ID number that is unique across all processes and threads on the system
	// TIDs are reused, but we'll assume they are unique over short periods of time (two update cycles)
	// the "_Total/_Total" and "Idle/n" instance names have PIDs and TIDs of 0
	//
	// we need unique names to match threads between succesive counter data queries
	// simply replacing the instance name with the TID would be (mostly) sufficient, but not very human friendly
	// this routine strips the "/n" thread instance number, and adds "#tid" to make the names unique
	// there are two exceptions to this:
	//   1) there is only one instance of "_Total/_Total", so it is inherently unique
	//   2) the "Idle/n" threads *need* to retain their "/n" thread instance number to remain unique since their TIDs are 0
	//
	// counter "Elapsed" returns 0 for "_Total/_Total", and odd, large numbers for "Idle/n" and some "System/n" threads

	// Unique names will be longer than original names since "/n" will be removed but "#tid" appended.
	// we'll reserve 10 extra symbols for each name to be safe (we'll ignore the savings from L"/n").
	// Display names will be shorter than original names since "/n" will be removed.

	namesCurrent.ensureElementsCount(itemCountCurrent);

	const size_t originalNamesSize = rawBuffersCurrent.getBufferSize() - sizeof(PDH_RAW_COUNTER_ITEM_W) * itemCountCurrent;
	namesBufferCurrent.ensureSize(originalNamesSize * 3 + static_cast<size_t>(itemCountCurrent) * 10 * sizeof(wchar_t));
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		const wchar_t* namePtr = idRawBuffer[instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;

		// generate a unique name
		item.uniqueName = namesBuffer;
		const long long tid = idRawBuffer[instanceInx].RawValue.FirstValue;
		if (tid == 0) {
			// special case needed for "_Total/_Total" and "Idle/n", we'll still add "#0" for consistency
			while (true) {
				const wchar_t c = *namePtr;
				*namesBuffer = c;
				namesBuffer++;
				namePtr++;
				if (c == L'\0') {
					break;
				}
			}
		} else {
			// keep the instance name, but strip off the "/n" thread instance number and tack on "#tid"
			while (true) {
				wchar_t c = *namePtr;
				if (c == L'/') {
					c = L'\0';
				}
				*namesBuffer = c;
				namesBuffer++;
				namePtr++;
				if (c == L'\0') {
					break;
				}
			}
		}
		const int length = swprintf(namesBuffer, 11, L"%I64u#", tid);
		namesBuffer += length;

		// generate a display name
		item.displayName = namesBuffer;
		// keep the instance name, but strip off the "/n" thread instance number
		// no special case needed for "_Total/_Total" and "Idle/n"
		while (true) {
			wchar_t c = *namePtr;
			if (c == L'/') {
				c = L'\0';
			}
			*namesBuffer = c;
			namesBuffer++;
			namePtr++;
			if (c == L'\0') {
				break;
			}
		}

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::modifyNameLogicalDiskDriveLetter() {
	// Strip anything aside drive letter and colon after it. E.g., "C:\path" -> "C:".
	// Ignore volumes that are not mounted ("HardDiskVolume#").

	namesCurrent.ensureElementsCount(itemCountCurrent);

	const size_t originalNamesSize = rawBuffersCurrent.getBufferSize() - sizeof(PDH_RAW_COUNTER_ITEM_W) * itemCountCurrent;
	namesBufferCurrent.ensureSize(originalNamesSize + static_cast<size_t>(itemCountCurrent) * 3 * sizeof(wchar_t));
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		const wchar_t* namePtr = rawBuffersCurrent[0][instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;
		item.uniqueName = namePtr;

		if (namePtr[1] == L':') {
			item.displayName = namesBuffer;
			namesBuffer[0] = namePtr[0];
			namesBuffer[1] = L':';
			namesBuffer[2] = L'\0';
			namesBuffer += 3;
		} else if (wcsstr(namesBuffer, L"HarddiskVolume") != nullptr) {
			item.displayName = namesBuffer;
			while (true) {
				wchar_t c = *namePtr;
				if (!std::iswalpha(c)) {
					*namesBuffer = L'\0';
					namesBuffer++;
					break;
				}
				*namesBuffer = c;
				namesBuffer++;
				namePtr++;
				if (c == L'\0') {
					break;
				}
			}
		} else {
			item.displayName = namePtr;
		}

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::modifyNameLogicalDiskMountPath() {
	// Leave only folder in mount path. E.g., "C:\path\mount" -> "C:\path".
	// Ignore volumes that are not mounted ("HardDiskVolume#").

	namesCurrent.ensureElementsCount(itemCountCurrent);

	const size_t originalNamesSize = rawBuffersCurrent.getBufferSize() - sizeof(PDH_RAW_COUNTER_ITEM_W) * itemCountCurrent;
	namesBufferCurrent.ensureSize(originalNamesSize * 2);
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		const wchar_t* namePtr = rawBuffersCurrent[0][instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;
		item.uniqueName = namePtr;

		if (namePtr[1] == L':') {
			item.displayName = namesBuffer;
			wcscpy(namesBuffer, namePtr);
			wchar_t* found = wcsrchr(namesBuffer, L'\\');
			if (found != nullptr) {
				found[1] = L'\0';
				namesBuffer = found + 2;
			} else {
				namesBuffer = wcsrchr(namesBuffer, L'\0') + 1;
			}
		} else if (wcsstr(namePtr, L"HarddiskVolume") != nullptr) {
			item.displayName = namesBuffer;
			while (true) {
				wchar_t c = *namePtr;
				if (!std::iswalpha(c)) {
					*namesBuffer = L'\0';
					namesBuffer++;
					break;
				}
				*namesBuffer = c;
				namesBuffer++;
				namePtr++;
				if (c == L'\0') {
					break;
				}
			}
		} else {
			item.displayName = namePtr;
		}

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::modifyNameGPUProcessName() {

	// objectNames "GPU Engine" and "GPU Process Memory" return unique instance names that contain PIDs
	// we'll make the names more friendly by substituting <processname>
	// display name will be process names

	namesCurrent.ensureElementsCount(itemCountCurrent);

	// build a map from PIDs to process names
	// considering the overhead of building the map, is this faster than linear searching?

	size_t maxProcessNameLength = 0;
	std::unordered_map<long long, LPWSTR> mapPIDsToNames(idItemCount);
	for (unsigned long instanceInx = 0; instanceInx < idItemCount; ++instanceInx) {
		const PDH_RAW_COUNTER_ITEM_W instance = idRawBuffer[instanceInx];
		const long long pid = instance.RawValue.FirstValue;
		mapPIDsToNames[pid] = instance.szName;
		size_t nameLength = wcslen(instance.szName) + 1;
		maxProcessNameLength = std::max(maxProcessNameLength, nameLength);
	}

	namesBufferCurrent.ensureSize(itemCountCurrent * maxProcessNameLength * sizeof(wchar_t) * 2);
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		wchar_t* namePtr = rawBuffersCurrent[0][instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;
		item.uniqueName = namePtr;

		item.displayName = namesBuffer;
		bool foundPID = false;
		const wchar_t* str = wcsstr(namePtr, L"pid_");
		if (str != nullptr) {
			const unsigned long pid = wcstoul(str + 4, nullptr, 10);
			const auto iter = mapPIDsToNames.find(pid);
			if (iter != mapPIDsToNames.end()) {
				// alternative that prepends <processname># instead of using <processname> only
				// adjust malloc accordingly
				// len = wsprintf((LPWSTR) lpNames, L"%s#%s", iter->second, lpNamesBuffer[instanceInx].originalName);
				wcscpy(namesBuffer, iter->second);
				namesBuffer += wcslen(namesBuffer) + 1;
				foundPID = true;
			}
		}

		if (!foundPID) {
			wcscpy(namesBuffer, namePtr);
			namesBuffer += wcslen(namesBuffer) + 1;
		}

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::modifyNameGPUEngtype() {

	// objectNames "GPU Engine" and "GPU Process Memory" return unique instance names that contain engtypes
	// we'll prepare the names for rollup by keeping engtype_x only
	//
	// since all counter buffers should contain the same names at the same indexes, we'll use index 0 for our "donor"

	namesCurrent.ensureElementsCount(itemCountCurrent);

	// display name will be shorter than original name since only engtype_x is kept
	// oldNameSize will be sufficient (we'll ignore the savings for simplicity)

	const size_t originalNamesSize = rawBuffersCurrent.getBufferSize() - sizeof(PDH_RAW_COUNTER_ITEM_W) * itemCountCurrent;
	namesBufferCurrent.ensureSize(originalNamesSize * 2);
	wchar_t* namesBuffer = namesBufferCurrent.getPointer();

	for (unsigned long instanceInx = 0; instanceInx < itemCountCurrent; ++instanceInx) {
		wchar_t* namePtr = rawBuffersCurrent[0][instanceInx].szName;
		modifiedNameItem& item = namesCurrent[instanceInx];
		item.originalName = namePtr;
		item.uniqueName = namePtr;

		const wchar_t* str = wcsstr(namePtr, L"engtype_");
		if (str != nullptr) {
			item.displayName = namesBuffer;
			wcscpy(namesBuffer, str);
			const size_t length = wcslen(namesBuffer);
			namesBuffer += length + 1;
		} else {
			item.displayName = namePtr;
		}

		item.searchName = namesBuffer;
		namesBuffer += copyUpperCase(item.displayName, namesBuffer);
	}
}
void pmr::ParentData::createModifiedNames() {
	if (generateNamesFunction == nullptr) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"name modification function not specified");
		typeHolder->temporaryBroken = true;
		modifyNameNone();
		return;
	}
	(this->*generateNamesFunction)();
}

long pmr::ParentData::findPreviousName(const unsigned long currentIndex) const {
	// try to find a match for the current instance name in the previous names buffer
	// use the unique name for this search because we need a unique match
	// counter buffers tend to be *mostly* aligned, so we'll try to short-circuit a full search

	// try for a direct hit
	unsigned long previousInx = currentIndex;
	if (previousInx >= itemCountPrevious)
		previousInx = itemCountPrevious - 1;

	if (wcscmp(namesPrevious[previousInx].uniqueName, namesCurrent[currentIndex].uniqueName) == 0)
		return previousInx;

	// try a window around currentIndex
	constexpr int windowSize = 5;

	long startInx = currentIndex - windowSize;
	if (startInx < 0)
		startInx = 0;
	unsigned long endInx = currentIndex + windowSize;
	if (endInx >= itemCountPrevious)
		endInx = itemCountPrevious - 1;

	for (previousInx = static_cast<unsigned long>(startInx); previousInx <= endInx; ++previousInx) {
		if (wcscmp(namesPrevious[previousInx].uniqueName, namesCurrent[currentIndex].uniqueName) == 0)
			return previousInx;
	}

	// no luck, search the entire array
	for (previousInx = 0; previousInx < itemCountPrevious; ++previousInx) {
		if (wcscmp(namesPrevious[previousInx].uniqueName, namesCurrent[currentIndex].uniqueName) == 0)
			return previousInx;
	}

	return -1;
}

inline long long pmr::ParentData::calculateRaw(const unsigned counterIndex, const indexesItem& originalIndexes) const {
	return rawBuffersCurrent.getBuffer(counterIndex)[originalIndexes.originalCurrentInx].RawValue.FirstValue;
}
double pmr::ParentData::calculateFormatted(unsigned counterIndex, const indexesItem& originalIndexes) const {
	PDH_FMT_COUNTERVALUE formattedValue;
	return extractFormattedValue(hCounter[counterIndex], rawBuffersCurrent.getBuffer(counterIndex), rawBuffersPrevious.getBuffer(counterIndex), originalIndexes, formattedValue);
}
double pmr::ParentData::calculateTotal(const TotalSource source, const unsigned counterIndex, const pmre::RollupFunction rollupFunction) const {
	switch (source) {
	case TotalSource::RAW_COUNTER: return calculateTotal<&ParentData::calculateRaw>(rollupFunction, counterIndex);
	case TotalSource::FORMATTED_COUNTER: return calculateTotal<&ParentData::calculateFormatted>(rollupFunction, counterIndex);
	case TotalSource::EXPRESSION: return calculateExpressionTotal<&ParentData::calculateExpression<&ParentData::resolveReference>>(rollupFunction, expressions[counterIndex]);
	case TotalSource::ROLLUP_EXPRESSION: return calculateExpressionTotal<&ParentData::calculateExpression<&ParentData::resolveRollupReference>>(rollupFunction, expressions[counterIndex]);
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected TotalSource %d", source);
		return 0;
	}
}

const pmr::instanceKeyItem* pmr::ParentData::findAndCacheName(const pmrexp::reference& ref, const bool useRollup) const {
	auto& map = useRollup ? foundNamesRollup : foundNamesInstances;
	const decltype(foundNamesInstances)::key_type key(ref.useOrigName, ref.discarded, ref.name);
	const auto iter = map.find(key);
	if (iter != map.end() && !(map.key_comp()(key, iter->first))) {
		// key found.
		return iter->second;
	}
	// insert into found position
	const instanceKeyItem* item = findInstanceByName(ref, useRollup);
	map.insert(iter, decltype(foundNamesInstances)::value_type(key, item));
	return item;
}
double pmr::ParentData::calculateAndCacheTotal(const TotalSource source, const unsigned int counterIndex, const pmre::RollupFunction rollupFunction) const {
	const decltype(totalsCache)::key_type key(source, counterIndex, rollupFunction);
	const auto iter = totalsCache.find(key);
	if (iter != totalsCache.end() && !(totalsCache.key_comp()(key, iter->first))) {
		// key found.
		return iter->second;
	}
	// calculate and insert into found position
	const double value = calculateTotal(source, counterIndex, rollupFunction);
	totalsCache.insert(iter, decltype(totalsCache)::value_type(key, value));
	return value;
}

double pmr::ParentData::resolveReference(const pmrexp::reference& ref) const {
	if (ref.type == pmrexp::ReferenceType::UNKNOWN) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown reference being solved", sortBy);
		return 0.0;
	}
	if (ref.type == pmrexp::ReferenceType::COUNT) {
		if (ref.total) {
			return calculateCountTotal(ref.rollupFunction);
		}
		if (!ref.named) {
			return 1;
		}
		return findAndCacheName(ref, false) != nullptr;
	}
	if (ref.type == pmrexp::ReferenceType::COUNTER_RAW || ref.type == pmrexp::ReferenceType::COUNTER_FORMATTED) {
		if (ref.type == pmrexp::ReferenceType::COUNTER_FORMATTED && !canGetFormatted()) {
			return 0.0;
		}
		if (ref.total) {
			if (ref.type == pmrexp::ReferenceType::COUNTER_RAW) {
				return calculateAndCacheTotal(TotalSource::RAW_COUNTER, ref.counter, ref.rollupFunction);
			} else {
				return calculateAndCacheTotal(TotalSource::FORMATTED_COUNTER, ref.counter, ref.rollupFunction);
			}
		}
		if (!ref.named) {
			if (ref.type == pmrexp::ReferenceType::COUNTER_RAW) {
				return static_cast<double>(calculateRaw(ref.counter, expCurrentItem->originalIndexes));
			} else {
				return calculateFormatted(ref.counter, expCurrentItem->originalIndexes);
			}
		}
		const auto instance = findAndCacheName(ref, false);
		if (instance == nullptr) {
			return 0.0;
		}
		if (ref.type == pmrexp::ReferenceType::COUNTER_RAW) {
			return static_cast<double>(calculateRaw(ref.counter, instance->originalIndexes));
		} else {
			return calculateFormatted(ref.counter, instance->originalIndexes);
		}
	}
	if (ref.type == pmrexp::ReferenceType::EXPRESSION) {
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::EXPRESSION, ref.counter, ref.rollupFunction);
		}
		if (!ref.named) {
			return calculateExpression<&ParentData::resolveReference>(expressions[ref.counter]);
		}
		const auto instance = findAndCacheName(ref, false);
		if (instance == nullptr) {
			return 0.0;
		}
		const instanceKeyItem* const savedInstance = expCurrentItem;
		expCurrentItem = instance;
		const double res = calculateExpression<&ParentData::resolveReference>(expressions[ref.counter]);
		expCurrentItem = savedInstance;
		return res;
	}
	RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected reference type in resolveReference(): %d", ref.type);
	return 0.0;
}
double pmr::ParentData::calculateExpressionRollup(const pmrexp::ExpressionTreeNode& expression, const pmre::RollupFunction rollupFunction) const {
	const instanceKeyItem& instance = *expCurrentItem;
	instanceKeyItem tmp;		// we only need instanceKeyItem::originalIndexes member to solve usual expressions 
								// but let's use whole InstanceKeyItem
	expCurrentItem = &tmp;
	tmp.originalIndexes = instance.originalIndexes;
	double value = calculateExpression<&ParentData::resolveReference>(expression);

	switch (rollupFunction) {
	case pmre::RollupFunction::SUM:
	{
		for (const auto& item : instance.vectorIndexes) {
			tmp.originalIndexes = item;
			value += calculateExpression<&ParentData::resolveReference>(expression);
		}
		break;
	}
	case pmre::RollupFunction::AVERAGE:
	{
		for (const auto& item : instance.vectorIndexes) {
			tmp.originalIndexes = item;
			value += calculateExpression<&ParentData::resolveReference>(expression);
		}
		value /= (instance.vectorIndexes.size() + 1);
		break;
	}
	case pmre::RollupFunction::MINIMUM:
	{
		for (const auto& indexes : instance.vectorIndexes) {
			tmp.originalIndexes = indexes;
			value = std::min(value, calculateExpression<&ParentData::resolveReference>(expression));
		}
		break;
	}
	case pmre::RollupFunction::MAXIMUM:
	{
		for (const auto& indexes : instance.vectorIndexes) {
			tmp.originalIndexes = indexes;
			value = std::max(value, calculateExpression<&ParentData::resolveReference>(expression));
		}
		break;
	}
	case pmre::RollupFunction::FIRST:
		break;
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown expression type: %d", expression.type);
		value = 0.0;
		break;
	}
	expCurrentItem = &instance;
	return value;
}
double pmr::ParentData::calculateCountTotal(const pmre::RollupFunction rollupFunction) const {
	switch (rollupFunction) {
	case pmre::RollupFunction::SUM: return static_cast<double>(vectorInstanceKeys.size());
	case pmre::RollupFunction::AVERAGE:
	case pmre::RollupFunction::MINIMUM:
	case pmre::RollupFunction::MAXIMUM:
	case pmre::RollupFunction::FIRST: return 1.0;
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown rollup function: %d", rollupFunction);
		return 0.0;
	}
}

double pmr::ParentData::calculateRollupCountTotal(const pmre::RollupFunction rollupFunction) const {
	switch (rollupFunction) {
	case pmre::RollupFunction::SUM: return static_cast<double>(vectorRollupKeys.size());
	case pmre::RollupFunction::AVERAGE: return static_cast<double>(vectorInstanceKeys.size()) / vectorRollupKeys.size();
	case pmre::RollupFunction::MINIMUM:
	{
		size_t min = std::numeric_limits<size_t>::max();
		for (const auto& item : vectorRollupKeys) {
			size_t val = item.vectorIndexes.size();
			min = min < val ? min : val;
		}
		return static_cast<double>(min);
	}
	case pmre::RollupFunction::MAXIMUM:
	{
		size_t max = 0;
		for (const auto& item : vectorRollupKeys) {
			size_t val = item.vectorIndexes.size();
			max = max > val ? max : val;
		}
		return static_cast<double>(max);
	}
	case pmre::RollupFunction::FIRST:
		if (!vectorRollupKeys.empty()) {
			return static_cast<double>(vectorRollupKeys[0].vectorIndexes.size());
		}
		return 0.0;
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown rollup function: %d", rollupFunction);
		return 0.0;
	}
}
double pmr::ParentData::resolveRollupReference(const pmrexp::reference& ref) const {
	if (ref.type == pmrexp::ReferenceType::UNKNOWN) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown reference being solved", sortBy);
		return 0.0;
	}
	if (ref.type == pmrexp::ReferenceType::COUNT) {
		if (ref.total) {
			return calculateRollupCountTotal(ref.rollupFunction);
		}
		if (!ref.named) {
			return static_cast<double>(expCurrentItem->vectorIndexes.size());
		}
		const auto instance = findAndCacheName(ref, true);
		return instance == nullptr ? 0 : static_cast<double>(instance->vectorIndexes.size());
	}
	if (ref.type == pmrexp::ReferenceType::COUNTER_RAW || ref.type == pmrexp::ReferenceType::COUNTER_FORMATTED) {
		if (ref.type == pmrexp::ReferenceType::COUNTER_FORMATTED && !canGetFormatted()) {
			return 0.0;
		}
		if (ref.total) {
			if (ref.type == pmrexp::ReferenceType::COUNTER_RAW) {
				return calculateAndCacheTotal(TotalSource::RAW_COUNTER, ref.counter, ref.rollupFunction);
			} else {
				return calculateAndCacheTotal(TotalSource::FORMATTED_COUNTER, ref.counter, ref.rollupFunction);
			}
		}
		if (!ref.named) {
			if (ref.type == pmrexp::ReferenceType::COUNTER_RAW) {
				return calculateRollup<&ParentData::calculateRaw>(ref.rollupFunction, ref.counter, *expCurrentItem);
			} else {
				return calculateRollup<&ParentData::calculateFormatted>(ref.rollupFunction, ref.counter, *expCurrentItem);
			}
		}
		const auto instance = findAndCacheName(ref, true);
		if (instance == nullptr) {
			return 0.0;
		}
		if (ref.type == pmrexp::ReferenceType::COUNTER_RAW) {
			return calculateRollup<&ParentData::calculateRaw>(ref.rollupFunction, ref.counter, *instance);
		} else {
			return calculateRollup<&ParentData::calculateFormatted>(ref.rollupFunction, ref.counter, *instance);
		}
	}
	if (ref.type == pmrexp::ReferenceType::EXPRESSION || ref.type == pmrexp::ReferenceType::ROLLUP_EXPRESSION) {
		if (ref.total) {
			if (ref.type == pmrexp::ReferenceType::EXPRESSION) {
				return calculateAndCacheTotal(TotalSource::EXPRESSION, ref.counter, ref.rollupFunction);
			} else {
				return calculateAndCacheTotal(TotalSource::ROLLUP_EXPRESSION, ref.counter, ref.rollupFunction);
			}
		}
		if (!ref.named) {
			if (ref.type == pmrexp::ReferenceType::EXPRESSION) {
				return calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
			} else {
				return calculateExpression<&ParentData::resolveRollupReference>(rollupExpressions[ref.counter]);
			}
		}
		const auto instance = findAndCacheName(ref, true);
		if (instance == nullptr) {
			return 0.0;
		}
		const instanceKeyItem* const savedInstance = expCurrentItem;
		expCurrentItem = instance;
		double res;
		if (ref.type == pmrexp::ReferenceType::EXPRESSION) {
			res = calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		} else {
			res = calculateExpression<&ParentData::resolveRollupReference>(rollupExpressions[ref.counter]);
		}
		expCurrentItem = savedInstance;
		return res;
	}
	RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected reference type in resolveReference(): %d", ref.type);
	return 0.0;
}

double pmr::ParentData::extractFormattedValue(
	const PDH_HCOUNTER hCounter,
	const PDH_RAW_COUNTER_ITEM_W* lpRawBufferCurrent, const PDH_RAW_COUNTER_ITEM_W* lpRawBufferPrevious,
	const indexesItem& item, PDH_FMT_COUNTERVALUE& formattedValue
) const {
	const PDH_STATUS pdhStatus = 
		PdhCalculateCounterFromRawValue(
			hCounter, 
			PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
			&(const_cast<PDH_RAW_COUNTER_ITEM_W*>(lpRawBufferCurrent)[item.originalCurrentInx].RawValue),
			&(const_cast<PDH_RAW_COUNTER_ITEM_W*>(lpRawBufferPrevious)[item.originalPreviousInx].RawValue), 
			&formattedValue);
	if (pdhStatus == ERROR_SUCCESS) {
		return formattedValue.doubleValue;
	}
	if (pdhStatus != PDH_CALC_NEGATIVE_VALUE && pdhStatus != PDH_CALC_NEGATIVE_DENOMINATOR && pdhStatus != PDH_CALC_NEGATIVE_TIMEBASE) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"PdhCalculateCounterFromRawValue failed, status=0x%x", pdhStatus);
		RmLogF(typeHolder->rm, LOG_ERROR, L"current '%s' raw=%I64u, previous '%s' raw=%I64u",
			lpRawBufferCurrent[item.originalCurrentInx].szName, lpRawBufferCurrent[item.originalCurrentInx].RawValue.FirstValue,
			lpRawBufferPrevious[item.originalPreviousInx].szName, lpRawBufferPrevious[item.originalPreviousInx].RawValue.FirstValue);
	}
	return 0.0;
}
