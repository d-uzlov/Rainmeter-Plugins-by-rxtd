/*
 * Copyright (C) 2018-2020 rxtd
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
		logger.error(L"Parent must be specified");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
	parent = utils::ParentBase::find<PerfmonParent>(rain.getSkin(), parentName);

	if (parent == nullptr) {
		logger.error(L"Parent '{}' doesn't exist", parentName);
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	if (parent->getState() == utils::MeasureState::eBROKEN) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
}

void PerfmonChild::vReload() {
	if (parent->getState() == utils::MeasureState::eTEMP_BROKEN) {
		setMeasureState(utils::MeasureState::eTEMP_BROKEN);
		return;
	}

	instanceIndex = rain.read(L"InstanceIndex").asInt<item_t>();
	ref.counter = rain.read(L"CounterIndex").asInt<counter_t>();
	ref.useOrigName = rain.read(L"SearchOriginalName").asBool();
	ref.total = rain.read(L"Total").asBool();
	ref.discarded = rain.read(L"Discarded").asBool();

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
		logger.warning(L"Type 'GetInstanceCount' is deprecated, set to 'GetCount' with Total=1 and RollupFunction=Sum");
		ref.type = ReferenceType::COUNT;
		ref.total = true;
		ref.rollupFunction = RollupFunction::eSUM;
		needReadRollupFunction = false;
	} else if (type == L"GetCount")
		ref.type = ReferenceType::COUNT;
	else if (type == L"GetInstanceName") {
		logger.warning(L"Type 'GetInstanceName' is deprecated, set to 'GetCount' with Total=0 and ResultString not Number");
		ref.type = ReferenceType::COUNT;
		ref.total = false;
		ref.rollupFunction = RollupFunction::eFIRST;
		resultStringType = ResultString::eDISPLAY_NAME;
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
		logger.error(L"Type '{}' is invalid for child measure", type);
		setMeasureState(utils::MeasureState::eTEMP_BROKEN);
		return;
	}

	if (needReadRollupFunction) {
		auto rollupFunctionStr = rain.readString(L"RollupFunction") % ciView();
		if (rollupFunctionStr.empty() || rollupFunctionStr == L"Sum") {
			ref.rollupFunction = RollupFunction::eSUM;
		} else if (rollupFunctionStr == L"Average") {
			ref.rollupFunction = RollupFunction::eAVERAGE;
		} else if (rollupFunctionStr == L"Minimum") {
			ref.rollupFunction = RollupFunction::eMINIMUM;
		} else if (rollupFunctionStr == L"Maximum") {
			ref.rollupFunction = RollupFunction::eMAXIMUM;
		} else if (rollupFunctionStr == L"Count") {
			logger.warning(L"RollupFunction 'Count' is deprecated, measure type set to 'GetCount'");
			ref.type = ReferenceType::COUNT;
		} else if (rollupFunctionStr == L"First") {
			ref.rollupFunction = RollupFunction::eFIRST;
		} else {
			logger.error(L"RollupFunction '{}' is invalid, set to 'Sum'", rollupFunctionStr);
			ref.rollupFunction = RollupFunction::eSUM;
		}
	}

	const auto resultStringStr = rain.readString(L"ResultString") % ciView();
	if (!forceUseName && (resultStringStr.empty() || resultStringStr == L"Number")) {
		resultStringType = ResultString::eNUMBER;
		setUseResultString(false);
	} else if (resultStringStr == L"OriginalName" || resultStringStr == L"OriginalInstanceName") {
		resultStringType = ResultString::eORIGINAL_NAME;
		setUseResultString(true);
	} else if (resultStringStr == L"UniqueName" || resultStringStr == L"UniqueInstanceName") {
		resultStringType = ResultString::eUNIQUE_NAME;
		setUseResultString(true);
	} else if (resultStringStr == L"DisplayName" || resultStringStr == L"DisplayInstanceName") {
		resultStringType = ResultString::eDISPLAY_NAME;
		setUseResultString(true);
	} else if (resultStringStr == L"RollupInstanceName") {
		logger.warning(L"ResultString 'RollupInstanceName' is deprecated, set to 'DisplayName'");
		resultStringType = ResultString::eDISPLAY_NAME;
		setUseResultString(true);
	} else if (forceUseName) {
		resultStringType = ResultString::eDISPLAY_NAME;
		setUseResultString(true);
	} else {
		logger.error(L"ResultString '{}' is invalid, set to 'Number'", resultStringStr);
		resultStringType = ResultString::eNUMBER;
		setUseResultString(false);
	}

	ref.named = ref.useOrigName || !ref.name.empty();
}

double PerfmonChild::vUpdate() {
	if (!parent->canGetRaw() || ref.type == ReferenceType::COUNTER_FORMATTED && !parent->canGetFormatted()) {
		return 0;
	}

	if (ref.total) {
		return parent->getValue(ref, nullptr, logger);
	}

	const InstanceInfo* instance = parent->findInstance(ref, instanceIndex);
	if (instance == nullptr) {
		return 0;
	}

	return parent->getValue(ref, instance, logger);
}

void PerfmonChild::vUpdateString(string& resultStringBuffer) {
	if (ref.total) {
		resultStringBuffer = L"Total";
		return;
	}
	if (instance == nullptr) {
		return;
	}
	resultStringBuffer = parent->getInstanceName(*instance, resultStringType);
}
