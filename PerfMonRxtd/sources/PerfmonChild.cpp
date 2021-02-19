/*
 * Copyright (C) 2018-2021 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PerfmonChild.h"

using namespace rxtd::perfmon;

PerfmonChild::PerfmonChild(Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	auto parentName = rain.read(L"Parent").asIString();
	if (parentName.empty()) {
		logger.error(L"Parent must be specified");
		throw std::runtime_error{ "" };
	}
	parent = utils::ParentBase::find<PerfmonParent>(rain.getSkin(), parentName);

	if (parent == nullptr) {
		logger.error(L"Parent '{}' is not found", parentName);
		throw std::runtime_error{ "" };
	}
}

void PerfmonChild::vReload() {
	instanceIndex = rain.read(L"InstanceIndex").asInt();
	ref.counter = rain.read(L"CounterIndex").asInt();
	ref.useOrigName = rain.read(L"SearchOriginalName").asBool();
	ref.total = rain.read(L"Total").asBool();
	ref.discarded = rain.read(L"Discarded").asBool();


	instanceName = rain.read(L"InstanceName").asString();
	if (!ref.useOrigName) {
		utils::StringUtils::makeUppercaseInPlace(instanceName);
	}
	ref.namePattern = MatchPattern{ instanceName };

	bool needReadRollupFunction = true;
	bool forceUseName = false;
	const auto type = rain.read(L"Type").asIString();
	if (type == L"GetInstanceCount") {
		logger.warning(L"Type 'GetInstanceCount' is deprecated, set to 'GetCount' with Total=1 and RollupFunction=Sum");
		ref.type = Reference::Type::COUNT;
		ref.total = true;
		ref.rollupFunction = RollupFunction::eSUM;
		needReadRollupFunction = false;
	} else if (type == L"GetCount")
		ref.type = Reference::Type::COUNT;
	else if (type == L"GetInstanceName") {
		logger.warning(L"Type 'GetInstanceName' is deprecated, set to 'GetCount' with Total=0");
		ref.type = Reference::Type::COUNT;
		ref.total = false;
		ref.rollupFunction = RollupFunction::eFIRST;
		resultStringType = ResultString::eDISPLAY_NAME;
		forceUseName = true;
	} else if (type == L"GetRawCounter") {
		ref.type = Reference::Type::COUNTER_RAW;
	} else if (type == L"GetFormattedCounter") {
		ref.type = Reference::Type::COUNTER_FORMATTED;
	} else if (type == L"GetExpression") {
		ref.type = Reference::Type::EXPRESSION;
	} else if (type == L"GetRollupExpression") {
		ref.type = Reference::Type::ROLLUP_EXPRESSION;
	} else {
		logger.error(L"Type '{}' is invalid for child measure", type);
		setInvalid();
		return;
	}

	if (needReadRollupFunction) {
		auto rollupFunctionStr = rain.read(L"RollupFunction").asIString(L"Sum");
		if (rollupFunctionStr == L"Count") {
			logger.warning(L"RollupFunction 'Count' is deprecated, measure type set to 'GetCount'");
			ref.type = Reference::Type::COUNT;
		} else {
			auto typeOpt = parseEnum<RollupFunction>(rollupFunctionStr);
			if (typeOpt.has_value()) {
				ref.rollupFunction = typeOpt.value();
			} else {
				logger.error(L"RollupFunction '{}' is invalid, set to 'Sum'", rollupFunctionStr);
				ref.rollupFunction = RollupFunction::eSUM;
			}
		}
	}

	const auto resultStringStr = rain.read(L"ResultString").asIString(forceUseName ? L"DisplayName" : L"Number");
	if (resultStringStr == L"Number") {
		resultStringType = ResultString::eNUMBER;
	} else if (resultStringStr == L"OriginalInstanceName") {
		resultStringType = ResultString::eORIGINAL_NAME;
	} else if (resultStringStr == L"UniqueInstanceName") {
		resultStringType = ResultString::eUNIQUE_NAME;
	} else if (resultStringStr == L"DisplayInstanceName") {
		resultStringType = ResultString::eDISPLAY_NAME;
	} else if (resultStringStr == L"RollupInstanceName") {
		logger.warning(L"ResultString 'RollupInstanceName' is deprecated, set to 'DisplayName'");
		resultStringType = ResultString::eDISPLAY_NAME;
	} else {
		auto typeOpt = parseEnum<ResultString>(resultStringStr);
		if (typeOpt.has_value()) {
			resultStringType = typeOpt.value();
		} else {
			logger.error(L"ResultString '{}' is invalid, set to 'Number'", resultStringStr);
			resultStringType = ResultString::eNUMBER;
		}
	}

	setUseResultString(resultStringType != ResultString::eNUMBER);
}

double PerfmonChild::vUpdate() {
	return parent->getValues(ref, instanceIndex, resultStringType, stringValue);
}

void PerfmonChild::vUpdateString(string& resultStringBuffer) {
	resultStringBuffer = stringValue;
}
