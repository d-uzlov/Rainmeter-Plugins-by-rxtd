/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PerfMonRXTD.h"
#include "RainmeterAPI.h"
#include "Child.h"

pmr::ChildData::ChildData(TypeHolder* typeHolder)
	: typeHolder(typeHolder) {
	// read fixed child options

	parentName = RmReadString(typeHolder->rm, L"Parent", L"");
	if (parentName == L"") {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Parent must be specified");
		typeHolder->broken = true;
		return;
	}

	// find parent measure

	for (auto& iter : parentMeasuresVector) {
		if ((iter->typeHolder->skin == typeHolder->skin) && (iter->typeHolder->measureName == parentName)) {
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

	LPCWSTR str;

	// read settable child options

	instanceIndex = RmReadInt(typeHolder->rm, L"InstanceIndex", 0);
	counterIndex = RmReadInt(typeHolder->rm, L"CounterIndex", 0);
	origName = RmReadInt(typeHolder->rm, L"SearchOriginalName", 0) != 0;

	bool needCheckCounterIndex = false;
	str = RmReadString(typeHolder->rm, L"Type", L"");
	if (_wcsicmp(str, L"GetInstanceCount") == 0)
		updateFunction = &ChildData::getInstanceCount;
	else if (_wcsicmp(str, L"GetInstanceName") == 0)
		updateFunction = &ChildData::getInstanceName;
	else if (_wcsicmp(str, L"GetRawCounter") == 0) {
		updateFunction = &ChildData::getRawCounter;
		needCheckCounterIndex = true;
	} else if (_wcsicmp(str, L"GetFormattedCounter") == 0) {
		updateFunction = &ChildData::getFormattedCounter;
		needCheckCounterIndex = true;
	} else if (_wcsicmp(str, L"GetExpression") == 0)
		updateFunction = &ChildData::getExpression;
	else if (_wcsicmp(str, L"GetRollupExpression") == 0)
		updateFunction = &ChildData::getRollupExpression;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"Type '%s' is invalid for child measure", str);
		typeHolder->temporaryBroken = true;
		updateFunction = nullptr;
		return;
	}

	const wchar_t* nameSource = RmReadString(typeHolder->rm, L"NameSource", L"");
	if (_wcsicmp(nameSource, L"") == 0 || _wcsicmp(nameSource, L"Passed") == 0)
		searchPlace = pmre::NSP_PASSED;
	else if (_wcsicmp(nameSource, L"Discarded") == 0)
		searchPlace = pmre::NSP_DISCARDED;
	else if (_wcsicmp(nameSource, L"PassedDiscarded") == 0)
		searchPlace = pmre::NSP_PASSED_DISCARDED;
	else if (_wcsicmp(nameSource, L"DiscardedPassed") == 0)
		searchPlace = pmre::NSP_DISCARDED_PASSED;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"NameSource '%s' is invalid, set to 'Passed'", nameSource);
		searchPlace = pmre::NSP_PASSED;
	}

	std::wstring instanceName = RmReadString(typeHolder->rm, L"InstanceName", L"");
	if (!origName) {
		CharUpperW(&instanceName[0]);
	}
	const auto len = instanceName.size();
	if (len >= 2 && instanceName[0] == L'*' && instanceName[len - 1] == L'*') {
		this->instanceName = instanceName.substr(1, len - 2);
		instanceNameMatchPartial = true;
	} else {
		this->instanceName = instanceName;
		instanceNameMatchPartial = false;
	}

	str = RmReadString(typeHolder->rm, L"RollupFunction", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Sum") == 0)
		rollupFunction = pmre::ROLLUP_SUM;
	else if (_wcsicmp(str, L"Average") == 0)
		rollupFunction = pmre::ROLLUP_AVERAGE;
	else if (_wcsicmp(str, L"Minimum") == 0)
		rollupFunction = pmre::ROLLUP_MINIMUM;
	else if (_wcsicmp(str, L"Maximum") == 0)
		rollupFunction = pmre::ROLLUP_MAXIMUM;
	else if (_wcsicmp(str, L"Count") == 0)
		rollupFunction = pmre::ROLLUP_COUNT;
	else {
		RmLogF(typeHolder->rm, LOG_ERROR, L"RollupFunction '%s' is invalid, set to 'Sum'", str);
		rollupFunction = pmre::ROLLUP_SUM;
	}

	str = RmReadString(typeHolder->rm, L"ResultString", L"");
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Number") == 0) {
		resultStringType = pmre::RESULTSTRING_NUMBER;
		typeHolder->resultString = nullptr;
	} else {
		typeHolder->resultString = resultString;
		if (_wcsicmp(str, L"OriginalName") == 0 || _wcsicmp(str, L"OriginalInstanceName") == 0)
			resultStringType = pmre::RESULTSTRING_ORIGINAL_NAME;
		else if (_wcsicmp(str, L"UniqueName") == 0 || _wcsicmp(str, L"UniqueInstanceName") == 0)
			resultStringType = pmre::RESULTSTRING_UNIQUE_NAME;
		else if (_wcsicmp(str, L"DisplayName") == 0 || _wcsicmp(str, L"DisplayInstanceName") == 0)
			resultStringType = pmre::RESULTSTRING_DISPLAY_NAME;
		else if (_wcsicmp(str, L"RollupInstanceName") == 0) {
			RmLogF(typeHolder->rm, LOG_WARNING, L"ResultString 'RollupInstanceName' is deprecated, set to 'DisplayName'");
			resultStringType = pmre::RESULTSTRING_DISPLAY_NAME;
		} else {
			RmLogF(typeHolder->rm, LOG_ERROR, L"ResultString '%s' is invalid, set to 'Number'", str);
			resultStringType = pmre::RESULTSTRING_NUMBER;
			typeHolder->resultString = nullptr;
		}
	}

	if (needCheckCounterIndex && !indexIsInBounds(counterIndex, 0, static_cast<int>(parent->getCountersCount() - 1))) {
		RmLogF(typeHolder->rm, LOG_ERROR, L"CounterIndex must be between 0 and %d, set to 0", parent->getCountersCount() - 1);
		counterIndex = 0;
	}
}

void pmr::ChildData::update() {
	typeHolder->resultDouble = 0;
	wcscpy_s(resultString, L"");
	if (parent->typeHolder->isBroken()) {
		return;
	}
	if (updateFunction == nullptr) {
		return;
	}
	(this->*updateFunction)();
}

void pmr::ChildData::getInstanceCount() {
	if (!parent->canGetRaw())
		return;

	typeHolder->resultDouble = static_cast<double>(parent->getCount());
}
void pmr::ChildData::getInstanceName() {
	if (!parent->canGetRaw())
		return;

	const instanceKeyItem* instance = parent->findInstance(instanceName, instanceIndex, instanceNameMatchPartial, origName, searchPlace);
	if (instance == nullptr)
		return;

	const wchar_t* name = parent->getName(*instance, resultStringType);
	if (name != nullptr) {
		wcscpy_s(resultString, name);
	}
	typeHolder->resultDouble = 1;
}
void pmr::ChildData::getRawCounter() {
	if (!parent->canGetRaw())
		return;

	const instanceKeyItem* instance = parent->findInstance(instanceName, instanceIndex, instanceNameMatchPartial, origName, searchPlace);
	if (instance == nullptr)
		return;

	const wchar_t* name = parent->getName(*instance, resultStringType);
	if (name != nullptr) {
		wcscpy_s(resultString, name);
	}
	typeHolder->resultDouble = parent->getRawValue(counterIndex, *instance, rollupFunction);
}
void pmr::ChildData::getFormattedCounter() {
	if (!parent->canGetRaw())
		return;

	const instanceKeyItem* instance = parent->findInstance(instanceName, instanceIndex, instanceNameMatchPartial, origName, searchPlace);
	if (instance == nullptr)
		return;

	const wchar_t* name = parent->getName(*instance, resultStringType);
	if (name != nullptr) {
		wcscpy_s(resultString, name);
	}

	if (!parent->canGetFormatted())
		return;
	typeHolder->resultDouble = parent->getFormattedValue(counterIndex, *instance, rollupFunction);
}
void pmr::ChildData::getExpression() {
	if (!parent->canGetRaw())
		return;

	const instanceKeyItem* instance = parent->findInstance(instanceName, instanceIndex, instanceNameMatchPartial, origName, searchPlace);
	if (instance == nullptr)
		return;

	const wchar_t* name = parent->getName(*instance, resultStringType);
	if (name != nullptr) {
		wcscpy_s(resultString, name);
	}

	typeHolder->resultDouble = parent->getExpressionValue(counterIndex, *instance, rollupFunction);
}
void pmr::ChildData::getRollupExpression() {
	if (!parent->canGetRaw() || !parent->isRollupEnabled())
		return;

	const instanceKeyItem* instance = parent->findInstance(instanceName, instanceIndex, instanceNameMatchPartial, origName, searchPlace);
	if (instance == nullptr)
		return;

	const wchar_t* name = parent->getName(*instance, resultStringType);
	if (name != nullptr) {
		wcscpy_s(resultString, name);
	}

	typeHolder->resultDouble = parent->getRollupExpressionValue(counterIndex, *instance, rollupFunction);
}
