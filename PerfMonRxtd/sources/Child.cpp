/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RainmeterAPI.h"
#include "PerfMonRXTD.h"
#include "Child.h"
#include "utils.h"

pmr::ChildData::ChildData(TypeHolder* const typeHolder)
	: typeHolder(typeHolder) {
	// read fixed child options

	std::wstring parentName = RmReadString(typeHolder->rm, L"Parent", L"");
	if (parentName.empty()) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Parent must be specified");
		typeHolder->broken = true;
		return;
	}

	// find parent measure

	for (auto& iter : parentMeasuresVector) {
		if (iter->typeHolder->skin == typeHolder->skin &&
			_wcsicmp(iter->typeHolder->measureName.c_str(), parentName.c_str()) == 0) {
			parent = iter;
			break;
		}
	}

	if (parent == nullptr) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Parent '%s' not found", parentName.c_str());
		typeHolder->broken = true;
		return;
	}

	if (parent->typeHolder->broken) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Parent '%s' is broken", parentName.c_str());
		typeHolder->broken = true;
		return;
	}
}

void pmr::ChildData::reload() {
	if (typeHolder->broken) {
		typeHolder->temporaryBroken = true;
		return;
	}
	typeHolder->temporaryBroken = false;

	// read settable child options

	instanceIndex = RmReadInt(typeHolder->rm, L"InstanceIndex", 0);
	ref.counter = RmReadInt(typeHolder->rm, L"CounterIndex", 0);
	ref.useOrigName = RmReadInt(typeHolder->rm, L"SearchOriginalName", 0) != 0;
	ref.total = RmReadInt(typeHolder->rm, L"Total", 0) != 0;

	std::wstring instanceName = RmReadString(typeHolder->rm, L"InstanceName", L"");
	if (!ref.useOrigName) {
		CharUpperW(&instanceName[0]);
	}
	const auto len = instanceName.size();
	if (len >= 2 && instanceName[0] == L'*' && instanceName[len - 1] == L'*') {
		ref.name = instanceName.substr(1, len - 2);
		ref.namePartialMatch = true;
	} else {
		ref.name = instanceName;
		ref.namePartialMatch = false;
	}

	bool needCheckCounterIndex = false;
	bool needReadRollupFunction = true;
	const wchar_t* type = RmReadString(typeHolder->rm, L"Type", L"");
	if (_wcsicmp(type, L"GetInstanceCount") == 0) {
		RmLog(typeHolder->rm, LOG_WARNING, L"Type 'GetInstanceCount' is deprecated, set to 'GetCount' with Total=1 and RollupFunction=Sum");
		ref.type = pmrexp::ReferenceType::COUNT;
		ref.total = true;
		ref.rollupFunction = pmre::RollupFunction::SUM;
		needReadRollupFunction = false;
	} else if (_wcsicmp(type, L"GetCount") == 0)
		ref.type = pmrexp::ReferenceType::COUNT;
	else if (_wcsicmp(type, L"GetInstanceName") == 0) {
		RmLog(typeHolder->rm, LOG_WARNING, L"Type 'GetInstanceName' is deprecated, set to 'GetCount' with Total=0");
		ref.type = pmrexp::ReferenceType::COUNT;
		ref.total = false;
		ref.rollupFunction = pmre::RollupFunction::FIRST;
	} else if (_wcsicmp(type, L"GetRawCounter") == 0) {
		ref.type = pmrexp::ReferenceType::COUNTER_RAW;
		needCheckCounterIndex = true;
	} else if (_wcsicmp(type, L"GetFormattedCounter") == 0) {
		ref.type = pmrexp::ReferenceType::COUNTER_FORMATTED;
		needCheckCounterIndex = true;
	} else if (_wcsicmp(type, L"GetExpression") == 0)
		ref.type = pmrexp::ReferenceType::EXPRESSION;
	else if (_wcsicmp(type, L"GetRollupExpression") == 0)
		ref.type = pmrexp::ReferenceType::ROLLUP_EXPRESSION;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Type '%s' is invalid for child measure", type);
		typeHolder->temporaryBroken = true;
		return;
	}

	ref.discarded = RmReadInt(typeHolder->rm, L"Discarded", 0) != 0;

	if (needReadRollupFunction) {
		const wchar_t* rollupFunctionStr = RmReadString(typeHolder->rm, L"RollupFunction", L"");
		if (_wcsicmp(rollupFunctionStr, L"") == 0 || _wcsicmp(rollupFunctionStr, L"Sum") == 0)
			ref.rollupFunction = pmre::RollupFunction::SUM;
		else if (_wcsicmp(rollupFunctionStr, L"Average") == 0)
			ref.rollupFunction = pmre::RollupFunction::AVERAGE;
		else if (_wcsicmp(rollupFunctionStr, L"Minimum") == 0)
			ref.rollupFunction = pmre::RollupFunction::MINIMUM;
		else if (_wcsicmp(rollupFunctionStr, L"Maximum") == 0)
			ref.rollupFunction = pmre::RollupFunction::MAXIMUM;
		else if (_wcsicmp(rollupFunctionStr, L"Count") == 0) {
			RmLogF(typeHolder->rm, LOG_WARNING, L"RollupFunction 'Count' is deprecated, measure type set to 'GetCount'");
			ref.type = pmrexp::ReferenceType::COUNT;
		} else if (_wcsicmp(rollupFunctionStr, L"First") == 0)
			ref.rollupFunction = pmre::RollupFunction::FIRST;
		else {
			RmLogF(typeHolder->rm, LOG_ERROR, L"RollupFunction '%s' is invalid, set to 'Sum'", rollupFunctionStr);
			ref.rollupFunction = pmre::RollupFunction::SUM;
		}
	}

	const wchar_t* const resultStringStr = RmReadString(typeHolder->rm, L"ResultString", L"");
	if (_wcsicmp(resultStringStr, L"") == 0 || _wcsicmp(resultStringStr, L"Number") == 0) {
		resultStringType = ResultString::NUMBER;
		typeHolder->resultString = nullptr;
	} else {
		typeHolder->resultString = resultString;
		if (_wcsicmp(resultStringStr, L"OriginalName") == 0 || _wcsicmp(resultStringStr, L"OriginalInstanceName") == 0)
			resultStringType = ResultString::ORIGINAL_NAME;
		else if (_wcsicmp(resultStringStr, L"UniqueName") == 0 || _wcsicmp(resultStringStr, L"UniqueInstanceName") == 0)
			resultStringType = ResultString::UNIQUE_NAME;
		else if (_wcsicmp(resultStringStr, L"DisplayName") == 0 || _wcsicmp(resultStringStr, L"DisplayInstanceName") == 0)
			resultStringType = ResultString::DISPLAY_NAME;
		else if (_wcsicmp(resultStringStr, L"RollupInstanceName") == 0) {
			RmLogF(typeHolder->rm, LOG_WARNING, L"ResultString 'RollupInstanceName' is deprecated, set to 'DisplayName'");
			resultStringType = ResultString::DISPLAY_NAME;
		} else {
			RmLogF(typeHolder->rm, LOG_ERROR, L"ResultString '%s' is invalid, set to 'Number'", resultStringStr);
			resultStringType = ResultString::NUMBER;
			typeHolder->resultString = nullptr;
		}
	}

	if (needCheckCounterIndex && !indexIsInBounds(ref.counter, 0, static_cast<int>(parent->getCountersCount() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"CounterIndex must be between 0 and %d, set to 0", parent->getCountersCount() - 1);
		ref.counter = 0;
	}

	if (!instanceName.empty() ||
		wcscmp(RmReadString(typeHolder->rm, L"NameSource", L""), L"") != 0 ||
		wcscmp(RmReadString(typeHolder->rm, L"SearchOriginalName", L""), L"") != 0) {
		ref.named = true;
	}
}

void pmr::ChildData::update() {
	typeHolder->resultDouble = 0;
	wcscpy_s(resultString, L"");

	if (!parent->canGetRaw())
		return;
	if (ref.type == pmrexp::ReferenceType::COUNTER_FORMATTED && !parent->canGetFormatted()) {
		return;
	}

	if (ref.total) {
		wcscpy_s(resultString, L"Total");
		typeHolder->resultDouble = parent->getValue(ref, nullptr, typeHolder->rm);
		return;
	}

	const instanceKeyItem* const instance = parent->findInstance(ref, instanceIndex);
	if (instance == nullptr)
		return;

	const wchar_t* const name = parent->getName(*instance, resultStringType);
	if (name != nullptr) {
		wcscpy_s(resultString, name);
	}
	typeHolder->resultDouble = parent->getValue(ref, instance, typeHolder->rm);
}
