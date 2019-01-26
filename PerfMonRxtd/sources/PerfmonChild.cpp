/* 
 * Copyright (C) 2018-2019 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PerfmonChild.h"

rxpm::PerfmonChild::PerfmonChild(rxu::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	std::wstring parentName = rain.readString(L"Parent");
	if (parentName.empty()) {
		log.error(L"Parent must be specified");
		setMeasureState(rxu::MeasureState::BROKEN);
		return;
	}
	parent = PerfmonParent::findInstance(rain.getSkin(), parentName.c_str());

	if (parent == nullptr) {
		log.error(L"Parent '{}' not found", parentName);
		setMeasureState(rxu::MeasureState::BROKEN);
		return;
	}

	if (parent->getState() == rxu::MeasureState::BROKEN) {
		log.error(L"Parent '{}' is broken", parentName);
		setMeasureState(rxu::MeasureState::BROKEN);
		return;
	}
}

void rxpm::PerfmonChild::_reload() {
	setMeasureState(rxu::MeasureState::WORKING);

	instanceIndex = rain.readInt(L"InstanceIndex");
	ref.counter = rain.readInt(L"CounterIndex");
	ref.useOrigName = rain.readBool(L"SearchOriginalName");
	ref.total = rain.readBool(L"Total");
	ref.discarded = rain.readBool(L"Discarded");

	std::wstring instanceName = rain.readString(L"InstanceName");
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

	bool needReadRollupFunction = true;
	bool forceUseName = false;
	const wchar_t* type = rain.readString(L"Type");
	if (_wcsicmp(type, L"GetInstanceCount") == 0) {
		log.warning(L"Type 'GetInstanceCount' is deprecated, set to 'GetCount' with Total=1 and RollupFunction=Sum");
		ref.type = ReferenceType::COUNT;
		ref.total = true;
		ref.rollupFunction = RollupFunction::SUM;
		needReadRollupFunction = false;
	} else if (_wcsicmp(type, L"GetCount") == 0)
		ref.type = ReferenceType::COUNT;
	else if (_wcsicmp(type, L"GetInstanceName") == 0) {
		log.warning(L"Type 'GetInstanceName' is deprecated, set to 'GetCount' with Total=0 and ResultString=DisplayName");
		ref.type = ReferenceType::COUNT;
		ref.total = false;
		ref.rollupFunction = RollupFunction::FIRST;
		resultStringType = ResultString::DISPLAY_NAME;
		forceUseName = true;
	} else if (_wcsicmp(type, L"GetRawCounter") == 0) {
		ref.type = ReferenceType::COUNTER_RAW;
	} else if (_wcsicmp(type, L"GetFormattedCounter") == 0) {
		ref.type = ReferenceType::COUNTER_FORMATTED;
	} else if (_wcsicmp(type, L"GetExpression") == 0)
		ref.type = ReferenceType::EXPRESSION;
	else if (_wcsicmp(type, L"GetRollupExpression") == 0)
		ref.type = ReferenceType::ROLLUP_EXPRESSION;
	else {
		log.error(L"Type '{}' is invalid for child measure", type);
		setMeasureState(rxu::MeasureState::TEMP_BROKEN);
		return;
	}

	if (needReadRollupFunction) {
		const wchar_t* rollupFunctionStr = rain.readString(L"RollupFunction");
		if (_wcsicmp(rollupFunctionStr, L"") == 0 || _wcsicmp(rollupFunctionStr, L"Sum") == 0)
			ref.rollupFunction = RollupFunction::SUM;
		else if (_wcsicmp(rollupFunctionStr, L"Average") == 0)
			ref.rollupFunction = RollupFunction::AVERAGE;
		else if (_wcsicmp(rollupFunctionStr, L"Minimum") == 0)
			ref.rollupFunction = RollupFunction::MINIMUM;
		else if (_wcsicmp(rollupFunctionStr, L"Maximum") == 0)
			ref.rollupFunction = RollupFunction::MAXIMUM;
		else if (_wcsicmp(rollupFunctionStr, L"Count") == 0) {
			log.warning(L"RollupFunction 'Count' is deprecated, measure type set to 'GetCount'");
			ref.type = ReferenceType::COUNT;
		} else if (_wcsicmp(rollupFunctionStr, L"First") == 0)
			ref.rollupFunction = RollupFunction::FIRST;
		else {
			log.error(L"RollupFunction '{}' is invalid, set to 'Sum'", rollupFunctionStr);
			ref.rollupFunction = RollupFunction::SUM;
		}
	}

	const wchar_t* const resultStringStr = rain.readString(L"ResultString");
	if (!forceUseName && (_wcsicmp(resultStringStr, L"") == 0 || _wcsicmp(resultStringStr, L"Number") == 0)) {
		resultStringType = ResultString::NUMBER;
	} else if (_wcsicmp(resultStringStr, L"OriginalName") == 0 || _wcsicmp(resultStringStr, L"OriginalInstanceName") == 0)
		resultStringType = ResultString::ORIGINAL_NAME;
	else if (_wcsicmp(resultStringStr, L"UniqueName") == 0 || _wcsicmp(resultStringStr, L"UniqueInstanceName") == 0)
		resultStringType = ResultString::UNIQUE_NAME;
	else if (_wcsicmp(resultStringStr, L"DisplayName") == 0 || _wcsicmp(resultStringStr, L"DisplayInstanceName") == 0)
		resultStringType = ResultString::DISPLAY_NAME;
	else if (_wcsicmp(resultStringStr, L"RollupInstanceName") == 0) {
		log.warning(L"ResultString 'RollupInstanceName' is deprecated, set to 'DisplayName'");
		resultStringType = ResultString::DISPLAY_NAME;
	} else if (forceUseName) {
		resultStringType = ResultString::DISPLAY_NAME;
	} else {
		log.error(L"ResultString '{}' is invalid, set to 'Number'", resultStringStr);
		resultStringType = ResultString::NUMBER;
	}

	ref.named = ref.useOrigName || !instanceName.empty();
}

const wchar_t*  rxpm::PerfmonChild::makeRetString() const {
	return resultStringType == ResultString::NUMBER ? nullptr : resultString.c_str();
}

std::tuple<double, const wchar_t*>  rxpm::PerfmonChild::_update() {
	resultString.clear();

	if (!parent->canGetRaw() || ref.type == ReferenceType::COUNTER_FORMATTED && !parent->canGetFormatted()) {
		return std::make_tuple(0, makeRetString());
	}

	if (ref.total) {
		resultString = L"Total";
		return std::make_tuple(parent->getValue(ref, nullptr, log), makeRetString());
	}

	const InstanceInfo* const instance = parent->findInstance(ref, instanceIndex);
	if (instance == nullptr) {
		return std::make_tuple(0, makeRetString());
	}

	resultString = parent->getInstanceName(*instance, resultStringType);
	return std::make_tuple(parent->getValue(ref, instance, log), makeRetString());
}
