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

#include "undef.h"

using namespace perfmon;

PerfmonChild::PerfmonChild(utils::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	auto parentName = rain.readString(L"Parent") % ciView() % own();
	if (parentName.empty()) {
		log.error(L"Parent must be specified");
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}
	parent = PerfmonParent::findInstance(rain.getSkin(), parentName);

	if (parent == nullptr) {
		log.error(L"Parent '{}' not found", parentName);
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}

	if (parent->getState() == utils::MeasureState::BROKEN) {
		log.error(L"Parent '{}' is broken", parentName);
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}
}

void PerfmonChild::_reload() {
	setMeasureState(utils::MeasureState::WORKING);

	instanceIndex = rain.readInt(L"InstanceIndex");
	ref.counter = rain.readInt(L"CounterIndex");
	ref.useOrigName = rain.readBool(L"SearchOriginalName");
	ref.total = rain.readBool(L"Total");
	ref.discarded = rain.readBool(L"Discarded");

	ref.name = rain.readString(L"InstanceName");
	if (!ref.useOrigName) {
		CharUpperW(&ref.name[0]);
	}
	const auto len = ref.name.size(); // TODO unite match record creation
	if (len >= 2 && ref.name.front() == L'*' && ref.name.back() == L'*') {
		utils::StringUtils::substringInplace(ref.name, 1, len - 2);
		ref.namePartialMatch = true;
	} else {
		ref.namePartialMatch = false;
	}

	bool needReadRollupFunction = true;
	bool forceUseName = false;
	const auto type = rain.readString(L"Type") % ciView();
	if (type == L"GetInstanceCount") {
		log.warning(L"Type 'GetInstanceCount' is deprecated, set to 'GetCount' with Total=1 and RollupFunction=Sum");
		ref.type = ReferenceType::COUNT;
		ref.total = true;
		ref.rollupFunction = RollupFunction::SUM;
		needReadRollupFunction = false;
	} else if (type == L"GetCount")
		ref.type = ReferenceType::COUNT;
	else if (type == L"GetInstanceName") {
		log.warning(L"Type 'GetInstanceName' is deprecated, set to 'GetCount' with Total=0 and ResultString not Number");
		ref.type = ReferenceType::COUNT;
		ref.total = false;
		ref.rollupFunction = RollupFunction::FIRST;
		resultStringType = ResultString::DISPLAY_NAME;
		forceUseName = true;
	} else if (type == L"GetRawCounter") {
		ref.type = ReferenceType::COUNTER_RAW;
	} else if (type == L"GetFormattedCounter") {
		ref.type = ReferenceType::COUNTER_FORMATTED;
	} else if (type == L"GetExpression") {
		ref.type = ReferenceType::EXPRESSION;
	} else if (type == L"GetRollupExpression") {
		ref.type = ReferenceType::ROLLUP_EXPRESSION;
	} else {
		log.error(L"Type '{}' is invalid for child measure", type);
		setMeasureState(utils::MeasureState::TEMP_BROKEN);
		return;
	}

	if (needReadRollupFunction) {
		auto rollupFunctionStr = rain.readString(L"RollupFunction") % ciView();
		if (rollupFunctionStr.empty() || rollupFunctionStr == L"Sum") {
			ref.rollupFunction = RollupFunction::SUM;
		} else if (rollupFunctionStr == L"Average") {
			ref.rollupFunction = RollupFunction::AVERAGE;
		} else if (rollupFunctionStr == L"Minimum") {
			ref.rollupFunction = RollupFunction::MINIMUM;
		} else if (rollupFunctionStr == L"Maximum") {
			ref.rollupFunction = RollupFunction::MAXIMUM;
		} else if (rollupFunctionStr == L"Count") {
			log.warning(L"RollupFunction 'Count' is deprecated, measure type set to 'GetCount'");
			ref.type = ReferenceType::COUNT;
		} else if (rollupFunctionStr == L"First") {
			ref.rollupFunction = RollupFunction::FIRST;
		} else {
			log.error(L"RollupFunction '{}' is invalid, set to 'Sum'", rollupFunctionStr);
			ref.rollupFunction = RollupFunction::SUM;
		}
	}

	const auto resultStringStr = rain.readString(L"ResultString") % ciView();
	if (!forceUseName && (resultStringStr.empty() || resultStringStr == L"Number")) {
		resultStringType = ResultString::NUMBER;
	} else if (resultStringStr == L"OriginalName" || resultStringStr == L"OriginalInstanceName") {
		resultStringType = ResultString::ORIGINAL_NAME;
	} else if (resultStringStr == L"UniqueName" || resultStringStr == L"UniqueInstanceName") {
		resultStringType = ResultString::UNIQUE_NAME;
	} else if (resultStringStr == L"DisplayName" || resultStringStr == L"DisplayInstanceName") {
		resultStringType = ResultString::DISPLAY_NAME;
	} else if (resultStringStr == L"RollupInstanceName") {
		log.warning(L"ResultString 'RollupInstanceName' is deprecated, set to 'DisplayName'");
		resultStringType = ResultString::DISPLAY_NAME;
	} else if (forceUseName) {
		resultStringType = ResultString::DISPLAY_NAME;
	} else {
		log.error(L"ResultString '{}' is invalid, set to 'Number'", resultStringStr);
		resultStringType = ResultString::NUMBER;
	}

	ref.named = ref.useOrigName || !ref.name.empty();
}

const wchar_t*  PerfmonChild::makeRetString() const {
	return resultStringType == ResultString::NUMBER ? nullptr : resultString.c_str();
}

std::tuple<double, const wchar_t*>  PerfmonChild::_update() {
	resultString.clear();

	if (!parent->canGetRaw() || ref.type == ReferenceType::COUNTER_FORMATTED && !parent->canGetFormatted()) {
		return std::make_tuple(0, makeRetString());
	}

	if (ref.total) {
		resultString = L"Total";
		return std::make_tuple(parent->getValue(ref, nullptr, log), makeRetString());
	}

	const auto instance = parent->findInstance(ref, instanceIndex);
	if (instance == nullptr) {
		return std::make_tuple(0, makeRetString());
	}

	resultString = parent->getInstanceName(*instance, resultStringType);
	return std::make_tuple(parent->getValue(ref, instance, log), makeRetString());
}
