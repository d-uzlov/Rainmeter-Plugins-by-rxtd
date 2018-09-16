/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#define _CRT_SECURE_NO_WARNINGS

#include <pdh.h>
#include <PdhMsg.h>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <functional>
#include <unordered_map>
#include <string>
#include <string_view>

#include "RainmeterAPI.h"
#include "expressions.h"
#include "PerfMonRXTD.h"
#include "Parent.h"

#undef max

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

	if ((objectName == L"Process") || (objectName == L"GPU Engine") || (objectName == L"GPU Process Memory"))
		getIDs = pmre::GETIDS_PIDS;
	else if (objectName == L"Thread")
		getIDs = pmre::GETIDS_TIDS;
	else
		getIDs = pmre::GETIDS_NONE;


	PDH_STATUS pdhStatus = PdhOpenQueryW(nullptr, 0, &(hQuery));
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

	if (getIDs != pmre::GETIDS_NONE) {
		std::wstring counterPath;
		switch (getIDs) {
		case pmre::GETIDS_PIDS:
			counterPath = L"\\Process(*)\\ID Process";
			break;
		case pmre::GETIDS_TIDS:
			counterPath = L"\\Thread(*)\\ID Thread";
			break;
		default:
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected getIDs %d", getIDs);
			typeHolder->broken = true;
			return;
		}

		pdhStatus = PdhAddEnglishCounterW(hQuery, counterPath.c_str(), 0, &hCounterID);
		if (pdhStatus != ERROR_SUCCESS) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhAddEnglishCounter failed, path='%s' status=0x%x", counterPath.c_str(), pdhStatus);
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

	LPCWSTR str = RmReadString(typeHolder->rm, L"SortBy", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"None") == 0)
		sortBy = pmre::SORTBY_NONE;
	else if (_wcsicmp(str, L"InstanceName") == 0)
		sortBy = pmre::SORTBY_INSTANCE_NAME;
	else if (_wcsicmp(str, L"RawCounter") == 0)
		sortBy = pmre::SORTBY_RAW_COUNTER;
	else if (_wcsicmp(str, L"FormattedCounter") == 0)
		sortBy = pmre::SORTBY_FORMATTED_COUNTER;
	else if (_wcsicmp(str, L"Expression") == 0)
		sortBy = pmre::SORTBY_EXPRESSION;
	else if (_wcsicmp(str, L"RollupExpression") == 0)
		sortBy = pmre::SORTBY_ROLLUP_EXPRESSION;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortBy '%s' is invalid, set to 'None'", str);
		sortBy = pmre::SORTBY_NONE;
	}

	str = RmReadString(typeHolder->rm, L"SortOrder", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Descending") == 0)
		sortOrder = pmre::SORTORDER_DESCENDING;
	else if (_wcsicmp(str, L"Ascending") == 0)
		sortOrder = pmre::SORTORDER_ASCENDING;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortOrder '%s' is invalid, set to 'Descending'", str);
		sortOrder = pmre::SORTORDER_DESCENDING;
	}

	str = RmReadString(typeHolder->rm, L"SortRollupFunction", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Sum") == 0)
		sortRollupFunction = pmre::ROLLUP_SUM;
	else if (_wcsicmp(str, L"Average") == 0)
		sortRollupFunction = pmre::ROLLUP_AVERAGE;
	else if (_wcsicmp(str, L"Minimum") == 0)
		sortRollupFunction = pmre::ROLLUP_MINIMUM;
	else if (_wcsicmp(str, L"Maximum") == 0)
		sortRollupFunction = pmre::ROLLUP_MAXIMUM;
	else if (_wcsicmp(str, L"Count") == 0)
		sortRollupFunction = pmre::ROLLUP_COUNT;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortRollupFunction '%s' is invalid, set to 'Sum'", str);
		sortRollupFunction = pmre::ROLLUP_SUM;
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
			node.type = pmrexp::EXP_TYPE_NUMBER;
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
			node.type = pmrexp::EXP_TYPE_NUMBER;
			node.number = 0;
			expressions.emplace_back(node);
			continue;
		}
		const int maxRURef = expression.maxExpRef();
		if (maxRURef >= 0) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Expressions can't reference RollupExpressions");
			pmrexp::ExpressionTreeNode node;
			node.type = pmrexp::EXP_TYPE_NUMBER;
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
			node.type = pmrexp::EXP_TYPE_NUMBER;
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
			node.type = pmrexp::EXP_TYPE_NUMBER;
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

	if ((sortBy == pmre::SORTBY_RAW_COUNTER || sortBy == pmre::SORTBY_FORMATTED_COUNTER)
		&& !indexIsInBounds(sortIndex, 0, static_cast<int>(hCounter.size() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"SortIndex must be between 0 and %d, set to 0", hCounter.size() - 1);
		sortIndex = 0;
	} else if (sortBy == pmre::SORTBY_EXPRESSION) {
		if (expressions.empty()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Sort by Expression requires at least 1 Expression specified. Set to None.");
			sortBy = pmre::SORTBY_NONE;
		} else if (!indexIsInBounds(sortIndex, 0, static_cast<int>(expressions.size() - 1))) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"SortIndex for Expression must be between 0 and %d, set to 0", expressions.size());
			sortIndex = 0;
		}
	} else if (sortBy == pmre::SORTBY_ROLLUP_EXPRESSION) {
		if (!rollup) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"RollupExpressions can't be used for sort if rollup is disabled. Set to None.");
			sortBy = pmre::SORTBY_NONE;
		} else if (rollupExpressions.empty()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"Sort by RollupExpression requires at least 1 RollupExpression specified. Set to None.");
			sortBy = pmre::SORTBY_NONE;
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

const pmr::instanceKeyItem* pmr::ParentData::findInstance(const std::wstring& name, unsigned long sortedIndex,
	bool instanceNameMatchPartial, bool useOrigName, pmre::nameSearchPlace searchPlace) const {
	if (name.empty()) {
		// get instance by index
		const std::vector<instanceKeyItem>& instances = rollup ? vectorRollupKeys : vectorInstanceKeys;
		sortedIndex += instanceIndexOffset;
		if ((sortedIndex < 0) || (sortedIndex >= instances.size()))
			return nullptr;
		return &instances[sortedIndex];
	}
	// get instance by name
	return findInstanceByName(name, instanceNameMatchPartial, useOrigName, rollup, searchPlace);
}

const pmr::instanceKeyItem* pmr::ParentData::findInstanceByNameInList(const std::wstring& name, bool instanceNameMatchPartial,
	bool useOrigName, const std::vector<instanceKeyItem>& instances) const {
	if (useOrigName) {
		for (const auto& vectorDiscardedKey : instances) {
			if (stringsMatch(namesCurrent[vectorDiscardedKey.originalIndexes.originalCurrentInx].originalName, name.c_str(), instanceNameMatchPartial)) {
				return &vectorDiscardedKey;
			}
		}
	} else {
		for (const auto& vectorDiscardedKey : instances) {
			if (stringsMatch(vectorDiscardedKey.sortName, name.c_str(), instanceNameMatchPartial)) {
				return &vectorDiscardedKey;
			}
		}
	}
	return nullptr;
}

const pmr::instanceKeyItem* pmr::ParentData::findInstanceByName(const std::wstring& name, bool instanceNameMatchPartial,
	bool useOrigName, bool useRollup, pmre::nameSearchPlace searchPlace) const {
	const instanceKeyItem* instance = nullptr;
	if (searchPlace == pmre::NSP_DISCARDED || searchPlace == pmre::NSP_DISCARDED_PASSED) {
		instance = findInstanceByNameInList(name, instanceNameMatchPartial, useOrigName, vectorDiscardedKeys);
		if (instance != nullptr) {
			return instance;
		}
		if (searchPlace == pmre::NSP_DISCARDED) {
			return nullptr;
		}
	}

	if (useRollup) {
		instance = findInstanceByNameInList(name, instanceNameMatchPartial, useOrigName, vectorRollupKeys);
	} else {
		instance = findInstanceByNameInList(name, instanceNameMatchPartial, useOrigName, vectorInstanceKeys);
	}
	if (instance != nullptr) {
		return instance;
	}

	if (searchPlace == pmre::NSP_PASSED_DISCARDED) {
		instance = findInstanceByNameInList(name, instanceNameMatchPartial, useOrigName, vectorDiscardedKeys);
		if (instance != nullptr) {
			return instance;
		}
	}
	// name not found
	return nullptr;
}

const wchar_t* pmr::ParentData::getName(const instanceKeyItem& instance, pmre::rsltStringType stringType) const {
	if (stringType == pmre::RESULTSTRING_NUMBER) {
		return L"";
	}
	if (instance.vectorIndexes.empty()) { // rollup instances have vector indexes filled and usual instances don't have
		const modifiedNameItem& name = namesCurrent[instance.originalIndexes.originalCurrentInx];
		switch (stringType) {
		case pmre::RESULTSTRING_ORIGINAL_NAME:
			return  name.originalName;
		case pmre::RESULTSTRING_UNIQUE_NAME:
			return  name.uniqueName;
		case pmre::RESULTSTRING_DISPLAY_NAME:
			return  name.displayName;
		default:
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected result string type %d", stringType);
			return L"";
		}
	} else {
		const modifiedNameItem& name = namesCurrent[instance.vectorIndexes[0].originalCurrentInx];
		switch (stringType) {
		case pmre::RESULTSTRING_ORIGINAL_NAME:
		case pmre::RESULTSTRING_UNIQUE_NAME:
		case pmre::RESULTSTRING_DISPLAY_NAME:
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
	if (getIDs != pmre::GETIDS_NONE) {
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
		// Let's keep all buffers. We only swap current and previous values so that we always have two snapshots of data
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

	if (!stopped || needUpdate) {
		needUpdate = false;

		referencesCache.clear();

		if (rawBuffersPrevious.isEmpty()) {
			buildInstanceKeysZero();
		} else {
			buildInstanceKeys();
		}
		if (rollup) {
			buildRollupKeys();
			sortInstanceKeys(vectorRollupKeys);
		} else {
			sortInstanceKeys(vectorInstanceKeys);
		}
	}

	std::wstring status = L"count=" + std::to_wstring(itemCountCurrent) + L"/" +
		std::to_wstring(vectorInstanceKeys.size()) + L"/" +
		std::to_wstring(vectorRollupKeys.size()) +
		L" sortby=" + std::to_wstring(sortBy) +
		L" rollup=" + (rollup ? L"1" : L"0") +
		L" stopped= " + (stopped ? L"1" : L"0");

	wcscpy_s(resultString, status.c_str());
}

void pmr::ParentData::setStopped(bool value) {
	stopped = value;
}
void pmr::ParentData::changeStopState() {
	stopped = !stopped;
}
void pmr::ParentData::setIndexOffset(int value) {
	if (value < 0) {
		instanceIndexOffset = 0;
	} else {
		instanceIndexOffset = value;
	}
}
int pmr::ParentData::getIndexOffset() const {
	return instanceIndexOffset;
}

void pmr::ParentData::sortInstanceKeys(std::vector<instanceKeyItem>& instances) {
	if (sortBy == pmre::SORTBY_NONE || instances.empty()) {
		return;
	}
	if (sortBy == pmre::SORTBY_INSTANCE_NAME) {
		switch (sortOrder) {
		case pmre::SORTORDER_ASCENDING:
			std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return wcscmp(lhs.sortName, rhs.sortName) > 0; });
			break;
		case pmre::SORTORDER_DESCENDING:
			std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return wcscmp(lhs.sortName, rhs.sortName) < 0; });
			break;
		default:
			RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected sortOrder %d", sortOrder);
			break;
		}
		return;
	}
	switch (sortBy) {
	case pmre::SORTBY_RAW_COUNTER:
	{
		if (instances[0].vectorIndexes.empty()) {
			PDH_RAW_COUNTER_ITEM_W* lpRawBufferCurrent = rawBuffersCurrent.getBuffer(sortIndex);
			for (auto& instance : instances) {
				instance.sortValue = static_cast<double>(lpRawBufferCurrent[instance.originalIndexes.originalCurrentInx].RawValue.FirstValue);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = static_cast<double>(calculateRawRollup(sortIndex, instance, sortRollupFunction));
			}
		}
		break;
	}
	case pmre::SORTBY_FORMATTED_COUNTER:
	{
		if (!canGetFormatted()) {
			for (auto& instance : instances) {
				instance.sortValue = 0.0;
			}
			break;
		}
		if (instances[0].vectorIndexes.empty()) {
			for (auto& instance : instances) {
				instance.sortValue = calculateFormatted(sortIndex, instance);
			}
			break;
		}
		for (auto& instance : instances) {
			instance.sortValue = calculateFormattedRollup(sortIndex, instance, sortRollupFunction);
		}
		break;
	}
	case pmre::SORTBY_EXPRESSION:
	{
		if (instances[0].vectorIndexes.empty()) {
			const pmrexp::ExpressionTreeNode& expression = expressions[sortIndex];
			for (unsigned int i = 0; i < instances.size(); i++) {
				expCurrentItem = &instances[i];
				instances[i].sortValue = resolveExpression(expression);
			}
		} else {
			const pmrexp::ExpressionTreeNode& expression = expressions[sortIndex];
			for (unsigned int i = 0; i < instances.size(); i++) {
				expCurrentItem = &instances[i];
				instances[i].sortValue = resolveExpressionRollup(expression, sortRollupFunction);
			}
		}
		break;
	}
	case pmre::SORTBY_ROLLUP_EXPRESSION:
	{
		if (instances[0].vectorIndexes.empty()) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"resolving RollupExpression without rollup", sortBy);
			return;
		}
		const pmrexp::ExpressionTreeNode& expression = rollupExpressions[sortIndex];
		for (unsigned int i = 0; i < instances.size(); i++) {
			expCurrentItem = &instances[i];
			instances[i].sortValue = resolveRollupExpression(expression);
		}
		break;
	}
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected sortBy %d", sortBy);
		return;
	}
	switch (sortOrder) {
	case pmre::SORTORDER_ASCENDING:
		std::sort(instances.begin(), instances.end(), [](instanceKeyItem &lhs, instanceKeyItem &rhs) {return lhs.sortValue < rhs.sortValue; });
		break;
	case pmre::SORTORDER_DESCENDING:
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

	instanceKeyItem instanceKey;
	for (decltype(itemCountCurrent) currentInx = 0; currentInx < itemCountCurrent; ++currentInx) {
		const modifiedNameItem& item = namesCurrent[currentInx];

		instanceKey.sortName = item.searchName;
		instanceKey.originalIndexes.originalCurrentInx = currentInx;
		instanceKey.originalIndexes.originalPreviousInx = 0;

		if (isInstanceAllowed(item.searchName, item.originalName)) {
			vectorInstanceKeys.push_back(instanceKey);
		} else {
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
		} else {
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
		if (item.vectorIndexes.empty()) { // this item has just been created, we need to initialize name
			item.sortName = instance.sortName;
		} // else this item already exists in the map
		item.vectorIndexes.emplace_back(indexes);
	}

	// copy rollupKeys to the measure's rollup vector because vectors can be sorted, but maps cannot
	vectorRollupKeys.clear();
	vectorRollupKeys.reserve(mapRollupKeys.size());
	for (auto& mapRollupKey : mapRollupKeys) {
		vectorRollupKeys.emplace_back(mapRollupKey.second);
	}
}

double pmr::ParentData::getRawValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) const {
	if (!indexIsInBounds(counterIndex, 0, static_cast<int>(hCounter.size() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Child measure is trying to get non-existing counter %d.", counterIndex);
		return 0.0;
	}
	if (instance.vectorIndexes.empty()) { // rollup instances have vector indexes filled and usual instances don't have
		return static_cast<double>(calculateRaw(counterIndex, instance));
	} else {
		return static_cast<double>(calculateRawRollup(counterIndex, instance, rollupType));
	}
}
double pmr::ParentData::getFormattedValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) {
	if (!indexIsInBounds(counterIndex, 0, static_cast<int>(hCounter.size() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Child measure is trying to get non-existing counter %d.", counterIndex);
		return 0.0;
	}
	if (instance.vectorIndexes.empty()) { // rollup instances have vector indexes filled and usual instances don't have
		return calculateFormatted(counterIndex, instance);
	} else {
		return calculateFormattedRollup(counterIndex, instance, rollupType);
	}
}
double pmr::ParentData::getExpressionValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) {
	if (!indexIsInBounds(counterIndex, 0, static_cast<int>(expressions.size() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Child measure is trying to get non-existing Expression %d.", counterIndex);
		return 0.0;
	}
	if (instance.vectorIndexes.empty()) { // rollup instances have vector indexes filled and usual instances don't have
		expCurrentItem = &instance;
		return resolveExpression(expressions[counterIndex]);
	} else {
		expCurrentItem = &instance;
		return resolveExpressionRollup(expressions[counterIndex], rollupType);
	}
}
double pmr::ParentData::getRollupExpressionValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) {
	if (!indexIsInBounds(counterIndex, 0, static_cast<int>(rollupExpressions.size() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Child measure is trying to get non-existing RollupExpression %d.", counterIndex);
		return 0.0;
	}
	if (instance.vectorIndexes.empty()) { // rollup instances have vector indexes filled and usual instances don't have
		RmLogF(typeHolder->rm, LOG_ERROR, L"RollupExpression can't be evaluated without rollup");
		freeBuffers();
		return 0.0;
	} else {
		expCurrentItem = &instance;
		return resolveRollupExpression(rollupExpressions[counterIndex]);
	}
}
unsigned int pmr::ParentData::getCount() const {
	if (rollup) {
		return static_cast<unsigned int>(vectorRollupKeys.size());
	} else {
		return static_cast<unsigned int>(vectorInstanceKeys.size());
	}
}

long long pmr::ParentData::copyUpperCase(const wchar_t* source, wchar_t* dest) {
	wchar_t* start = dest;
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
	namesBufferCurrent.ensureSize(originalNamesSize * 2 + itemCountCurrent * 10 * sizeof(wchar_t));
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
	namesBufferCurrent.ensureSize(originalNamesSize * 3 + itemCountCurrent * 10 * sizeof(wchar_t));
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
	namesBufferCurrent.ensureSize(originalNamesSize + itemCountCurrent * 3 * sizeof(wchar_t));
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

long pmr::ParentData::findPreviousName(unsigned long currentIndex) const {
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

inline long long pmr::ParentData::calculateRaw(unsigned counterIndex, const instanceKeyItem& instance) const {
	return rawBuffersCurrent.getBuffer(counterIndex)[instance.originalIndexes.originalCurrentInx].RawValue.FirstValue;
}
double pmr::ParentData::calculateFormatted(unsigned counterIndex, const instanceKeyItem& instance) {
	PDH_FMT_COUNTERVALUE formattedValue;
	return extractFormattedValue(hCounter[counterIndex], rawBuffersCurrent.getBuffer(counterIndex), rawBuffersPrevious.getBuffer(counterIndex), instance.originalIndexes, formattedValue);
}
double pmr::ParentData::calculateRawRollup(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType)const {
	const PDH_RAW_COUNTER_ITEM_W * lpRawBufferCurrent = rawBuffersCurrent.getBuffer(counterIndex);
	const std::vector<indexesItem>& indexes = instance.vectorIndexes;
	const auto count = indexes.size();

	switch (rollupType) {
	case pmre::ROLLUP_SUM:
	{
		long long sum = 0;
		for (int vectorInx = 0; vectorInx < count; ++vectorInx) {
			const int currentInx = indexes[vectorInx].originalCurrentInx;
			sum += lpRawBufferCurrent[currentInx].RawValue.FirstValue;
		}
		return static_cast<double>(sum);
	}
	case pmre::ROLLUP_AVERAGE:
	{
		long long sum = 0;
		for (int vectorInx = 0; vectorInx < count; ++vectorInx) {
			const int currentInx = indexes[vectorInx].originalCurrentInx;
			sum += lpRawBufferCurrent[currentInx].RawValue.FirstValue;
		}
		const double avg = static_cast<double>(sum) / static_cast<double>(count);
		return avg;
	}
	case pmre::ROLLUP_MINIMUM:
	{
		long long min = indexes[0].originalCurrentInx;
		for (int vectorInx = 1; vectorInx < count; ++vectorInx) {
			const int currentInx = indexes[vectorInx].originalCurrentInx;
			const long long val = lpRawBufferCurrent[currentInx].RawValue.FirstValue;
			min = val < min ? val : min;
		}
		return static_cast<double>(min);
	}
	case pmre::ROLLUP_MAXIMUM:
	{
		long long max = indexes[0].originalCurrentInx;
		for (int vectorInx = 1; vectorInx < count; ++vectorInx) {
			const int currentInx = indexes[vectorInx].originalCurrentInx;
			const long long val = lpRawBufferCurrent[currentInx].RawValue.FirstValue;
			max = val > max ? val : max;
		}
		return static_cast<double>(max);
	}
	case pmre::ROLLUP_COUNT:
		return static_cast<double>(count);
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected rollupType %d", rollupType);
		return 0;
	}
}
double pmr::ParentData::calculateFormattedRollup(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) {
	PDH_FMT_COUNTERVALUE formattedValue;
	const PDH_HCOUNTER hCounter = this->hCounter[counterIndex];
	PDH_RAW_COUNTER_ITEM_W * lpRawBufferCurrent = rawBuffersCurrent.getBuffer(counterIndex);
	PDH_RAW_COUNTER_ITEM_W * lpRawBufferPrevious = rawBuffersPrevious.getBuffer(counterIndex);

	const std::vector<indexesItem>& indexes = instance.vectorIndexes;
	const auto count = indexes.size();

	switch (rollupType) {
	case pmre::ROLLUP_SUM:
	{
		double sum = 0;
		for (unsigned int i = 0; i < count; ++i) {
			const double val = extractFormattedValue(hCounter, lpRawBufferCurrent, lpRawBufferPrevious, indexes[i], formattedValue);
			sum = sum + val;
		}
		return sum;
	}
	case pmre::ROLLUP_AVERAGE:
	{
		double sum = 0;
		for (unsigned int i = 0; i < count; ++i) {
			const double val = extractFormattedValue(hCounter, lpRawBufferCurrent, lpRawBufferPrevious, indexes[i], formattedValue);
			sum = sum + val;
		}
		const double avg = sum / count;
		return avg;
	}
	case pmre::ROLLUP_MINIMUM:
	{
		double min = extractFormattedValue(hCounter, lpRawBufferCurrent, lpRawBufferPrevious, indexes[0], formattedValue);
		for (unsigned int i = 1; i < count; ++i) {
			const double val = extractFormattedValue(hCounter, lpRawBufferCurrent, lpRawBufferPrevious, indexes[i], formattedValue);
			min = val < min ? val : min;
		}
		return min;
	}
	case pmre::ROLLUP_MAXIMUM:
	{
		double max = extractFormattedValue(hCounter, lpRawBufferCurrent, lpRawBufferPrevious, indexes[0], formattedValue);
		for (unsigned int i = 1; i < count; ++i) {
			const double val = extractFormattedValue(hCounter, lpRawBufferCurrent, lpRawBufferPrevious, indexes[i], formattedValue);
			max = val > max ? val : max;
		}
		return max;
	}
	case pmre::ROLLUP_COUNT:
		return static_cast<double>(count);
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected rollupType %d", rollupType);
		return 0;
	}
}

double pmr::ParentData::resolveExpression(const pmrexp::ExpressionTreeNode& expression) {
	switch (expression.type) {
	case pmrexp::EXP_TYPE_UNKNOWN:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown expression being solved", sortBy);
		return 0.0;
	case pmrexp::EXP_TYPE_NUMBER: return expression.number;
	case pmrexp::EXP_TYPE_SUM:
	{
		double value = 0;
		for (const pmrexp::ExpressionTreeNode& node : expression.nodes) {
			value += resolveExpression(node);
		}
		return value;
	}
	case pmrexp::EXP_TYPE_DIFF:
	{
		double value = resolveExpression(expression.nodes[0]);
		for (unsigned int i = 1; i < expression.nodes.size(); i++) {
			value -= resolveExpression(expression.nodes[i]);
		}
		return value;
	}
	case pmrexp::EXP_TYPE_INVERSE: return -resolveExpression(expression.nodes[0]);
	case pmrexp::EXP_TYPE_MULT:
	{
		double value = 1;
		for (const pmrexp::ExpressionTreeNode& node : expression.nodes) {
			value *= resolveExpression(node);
		}
		return value;
	}
	case pmrexp::EXP_TYPE_DIV:
	{
		double value = resolveExpression(expression.nodes[0]);
		for (unsigned int i = 1; i < expression.nodes.size(); i++) {
			const double denominator = resolveExpression(expression.nodes[i]);
			if (denominator == 0) {
				value = 0;
				break;
			}
			value /= denominator;
		}
		return value;
	}
	case pmrexp::EXP_TYPE_POWER: return std::pow(resolveExpression(expression.nodes[0]), resolveExpression(expression.nodes[1]));
	case pmrexp::EXP_TYPE_REF: return resolveReference(expression.ref);
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown expression type: %d", expression.type);
		return 0.0;
	}
}
double pmr::ParentData::resolveReference(const pmrexp::reference& ref) {
	if (ref.type == pmrexp::REF_TYPE_UNKNOWN) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown reference being solved", sortBy);
		return 0.0;
	}
	if (ref.type == pmrexp::REF_TYPE_COUNTER_RAW || ref.type == pmrexp::REF_TYPE_COUNTER_FORMATTED) {
		if (ref.type == pmrexp::REF_TYPE_COUNTER_FORMATTED && !canGetFormatted()) {
			return 0.0;
		}
		if (!ref.named) {
			if (ref.type == pmrexp::REF_TYPE_COUNTER_RAW) {
				return static_cast<double>(calculateRaw(ref.counter, *expCurrentItem));
			} else {
				return calculateFormatted(ref.counter, *expCurrentItem);
			}
		}
		const auto iter = referencesCache.find(ref.uniqueName);
		if (iter != referencesCache.end()) {
			return iter->second;
		}
		const instanceKeyItem* instance = findInstanceByName(ref.name, ref.namePartialMatch, ref.useOrigName, false, ref.searchPlace);
		if (instance == nullptr) {
			referencesCache[ref.uniqueName] = 0.0;
			return 0.0;
		}
		if (ref.type == pmrexp::REF_TYPE_COUNTER_RAW) {
			const auto res = static_cast<double>(calculateRaw(ref.counter, *instance));
			referencesCache[ref.uniqueName] = res;
			return res;
		} else {
			const double res = calculateFormatted(ref.counter, *instance);
			referencesCache[ref.uniqueName] = res;
			return res;
		}
	}
	if (ref.type == pmrexp::REF_TYPE_EXPRESSION) {
		if (!ref.named) {
			return resolveExpression(expressions[ref.counter]);
		}
		const auto iter = referencesCache.find(ref.uniqueName);
		if (iter != referencesCache.end()) {
			return iter->second;
		}
		const instanceKeyItem* instance = findInstanceByName(ref.name, ref.namePartialMatch, ref.useOrigName, false, ref.searchPlace);
		if (instance == nullptr) {
			referencesCache[ref.uniqueName] = 0.0;
			return 0.0;
		}
		const instanceKeyItem* savedInstance = expCurrentItem;
		expCurrentItem = instance;
		const double res = resolveExpression(expressions[ref.counter]);
		expCurrentItem = savedInstance;
		referencesCache[ref.uniqueName] = res;
		return res;
	}
	RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected reference type in resolveReference(): %d", ref.type);
	return 0.0;
}

double pmr::ParentData::resolveExpressionRollup(const pmrexp::ExpressionTreeNode& expression, pmre::rollupFunctionType rollupFunction) {
	const instanceKeyItem& instance = *expCurrentItem;
	instanceKeyItem tmp;
	expCurrentItem = &tmp;
	double value;

	switch (rollupFunction) {
	case pmre::ROLLUP_SUM:
	{
		double sum = 0.0;
		for (auto& item : instance.vectorIndexes) {
			tmp.originalIndexes = item;
			sum += resolveExpression(expression);
		}
		value = sum;
		break;
	}
	case pmre::ROLLUP_AVERAGE:
	{
		double sum = 0.0;
		for (auto& item : instance.vectorIndexes) {
			tmp.originalIndexes = item;
			sum += resolveExpression(expression);
		}
		value = sum / instance.vectorIndexes.size();
		break;
	}
	case pmre::ROLLUP_MINIMUM:
	{
		tmp.originalIndexes = instance.vectorIndexes[0];
		double min = resolveExpression(expression);
		for (unsigned int i = 1; i < instance.vectorIndexes.size(); ++i) {
			tmp.originalIndexes = instance.vectorIndexes[i];
			double val = resolveExpression(expression);
			min = min < val ? min : val;
		}
		value = min;
		break;
	}
	case pmre::ROLLUP_MAXIMUM:
	{
		tmp.originalIndexes = instance.vectorIndexes[0];
		double max = resolveExpression(expression);
		for (unsigned int i = 1; i < instance.vectorIndexes.size(); ++i) {
			tmp.originalIndexes = instance.vectorIndexes[i];
			double val = resolveExpression(expression);
			max = max > val ? max : val;
		}
		value = max;
		break;
	}
	case pmre::ROLLUP_COUNT:
		value = static_cast<double>(instance.vectorIndexes.size());
		break;
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown expression type: %d", expression.type);
		value = 0.0;
		break;
	}
	expCurrentItem = &instance;
	return value;
}
double pmr::ParentData::resolveRollupExpression(const pmrexp::ExpressionTreeNode& expression) {
	switch (expression.type) {
	case pmrexp::EXP_TYPE_UNKNOWN:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown expression being solved", sortBy);
		return 0.0;
	case pmrexp::EXP_TYPE_NUMBER: return expression.number;
	case pmrexp::EXP_TYPE_SUM:
	{
		double value = 0;
		for (const pmrexp::ExpressionTreeNode& node : expression.nodes) {
			value += resolveRollupExpression(node);
		}
		return value;
	}
	case pmrexp::EXP_TYPE_DIFF:
	{
		double value = resolveRollupExpression(expression.nodes[0]);
		for (unsigned int i = 1; i < expression.nodes.size(); i++) {
			value -= resolveRollupExpression(expression.nodes[i]);
		}
		return value;
	}
	case pmrexp::EXP_TYPE_INVERSE: return -resolveRollupExpression(expression.nodes[0]);
	case pmrexp::EXP_TYPE_MULT:
	{
		double value = 1;
		for (const pmrexp::ExpressionTreeNode& node : expression.nodes) {
			value *= resolveRollupExpression(node);
		}
		return value;
	}
	case pmrexp::EXP_TYPE_DIV:
	{
		double value = resolveRollupExpression(expression.nodes[0]);
		for (unsigned int i = 1; i < expression.nodes.size(); i++) {
			const double denominator = resolveRollupExpression(expression.nodes[i]);
			if (denominator == 0) {
				value = 0;
				break;
			}
			value /= denominator;
		}
		return value;
	}
	case pmrexp::EXP_TYPE_POWER: return std::pow(resolveRollupExpression(expression.nodes[0]), resolveRollupExpression(expression.nodes[1]));
	case pmrexp::EXP_TYPE_REF: return resolveRollupReference(expression.ref);
	default:
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown expression type: %d", expression.type);
		return 0.0;
	}
}
double pmr::ParentData::resolveRollupReference(const pmrexp::reference& ref) {
	if (ref.type == pmrexp::REF_TYPE_UNKNOWN) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"unknown reference being solved", sortBy);
		return 0.0;
	}
	if (ref.type == pmrexp::REF_TYPE_COUNTER_RAW || ref.type == pmrexp::REF_TYPE_COUNTER_FORMATTED) {
		if (ref.type == pmrexp::REF_TYPE_COUNTER_FORMATTED && !canGetFormatted()) {
			return 0;
		}
		if (!ref.named) {
			if (ref.type == pmrexp::REF_TYPE_COUNTER_RAW) {
				return calculateRawRollup(ref.counter, *expCurrentItem, ref.rollupFunction);
			} else {
				return calculateFormattedRollup(ref.counter, *expCurrentItem, ref.rollupFunction);
			}
		}
		const auto iter = referencesCache.find(ref.uniqueName);
		if (iter != referencesCache.end()) {
			return iter->second;
		}
		const instanceKeyItem* instance = findInstanceByName(ref.name, ref.namePartialMatch, ref.useOrigName, true, ref.searchPlace);
		if (instance == nullptr) {
			referencesCache[ref.uniqueName] = 0.0;
			return 0.0;
		}
		if (ref.type == pmrexp::REF_TYPE_COUNTER_RAW) {
			const double res = calculateRawRollup(ref.counter, *instance, ref.rollupFunction);
			referencesCache[ref.uniqueName] = res;
			return res;
		} else {
			const double res = calculateFormattedRollup(ref.counter, *instance, ref.rollupFunction);
			referencesCache[ref.uniqueName] = res;
			return res;
		}
	}
	if (ref.type == pmrexp::REF_TYPE_EXPRESSION || ref.type == pmrexp::REF_TYPE_ROLLUP_EXPRESSION) {
		if (!ref.named) {
			if (ref.type == pmrexp::REF_TYPE_EXPRESSION) {
				return resolveExpressionRollup(expressions[ref.counter], ref.rollupFunction);
			} else {
				return resolveRollupExpression(rollupExpressions[ref.counter]);
			}
		}
		const auto iter = referencesCache.find(ref.uniqueName);
		if (iter != referencesCache.end()) {
			return iter->second;
		}
		const instanceKeyItem* instance = findInstanceByName(ref.name, ref.namePartialMatch, ref.useOrigName, true, ref.searchPlace);
		if (instance == nullptr) {
			referencesCache[ref.uniqueName] = 0.0;
			return 0.0;
		}
		const instanceKeyItem* savedInstance = expCurrentItem;
		expCurrentItem = instance;
		double res;
		if (ref.type == pmrexp::REF_TYPE_EXPRESSION) {
			res = resolveExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		} else {
			res = resolveRollupExpression(rollupExpressions[ref.counter]);
		}
		expCurrentItem = savedInstance;
		referencesCache[ref.uniqueName] = res;
		return res;
	}
	RmLogF(typeHolder->rm, LOG_ERROR, L"unexpected reference type in resolveReference(): %d", ref.type);
	return 0.0;
}

double pmr::ParentData::extractFormattedValue(
	PDH_HCOUNTER hCounter,
	PDH_RAW_COUNTER_ITEM_W* lpRawBufferCurrent, PDH_RAW_COUNTER_ITEM_W* lpRawBufferPrevious,
	const indexesItem& item, PDH_FMT_COUNTERVALUE& formattedValue
) const {
	const PDH_STATUS pdhStatus = PdhCalculateCounterFromRawValue(hCounter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
		&(lpRawBufferCurrent[item.originalCurrentInx].RawValue), &(lpRawBufferPrevious[item.originalPreviousInx].RawValue), &formattedValue);
	if (pdhStatus != ERROR_SUCCESS) {
		if (pdhStatus != PDH_CALC_NEGATIVE_VALUE && pdhStatus != PDH_CALC_NEGATIVE_DENOMINATOR && pdhStatus != PDH_CALC_NEGATIVE_TIMEBASE) {
			RmLogF(typeHolder->rm, LOG_ERROR, L"PdhCalculateCounterFromRawValue failed, status=0x%x", pdhStatus);
			RmLogF(typeHolder->rm, LOG_ERROR, L"current '%s' raw=%I64u, previous '%s' raw=%I64u",
				lpRawBufferCurrent[item.originalCurrentInx].szName, lpRawBufferCurrent[item.originalCurrentInx].RawValue.FirstValue,
				lpRawBufferPrevious[item.originalPreviousInx].szName, lpRawBufferPrevious[item.originalPreviousInx].RawValue.FirstValue);
		}
		return 0.0;
	}
	return formattedValue.doubleValue;
}
