/* 
 * Copyright (C) 2018-2021 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <functional>
#include <unordered_map>

#include "PerfmonParent.h"
#include "option-parser/OptionList.h"

using namespace perfmon;

PerfmonParent::PerfmonParent(utils::Rainmeter&& _rain) : ParentBase(std::move(_rain)) {
	setUseResultString(true);

	bool success = pdhWrapper.init(logger);
	if (!success) {
		throw std::runtime_error{ "" };
	}

	instanceManager.setIndexOffset(rain.read(L"InstanceIndexOffset").asInt(), false);
}

void PerfmonParent::vReload() {
	needUpdate = true;

	objectName = rain.read(L"ObjectName").asString();

	if (objectName.empty()) {
		logger.error(L"ObjectName must be specified");
		setInvalid();
		return;
	}

	auto counterNames = rain.read(L"CounterList").asList(L'|');

	if (counterNames.empty()) {
		logger.error(L"CounterList must have at least one entry");
		setInvalid();
		return;
	}
	if (counterNames.size() > 30) {
		logger.error(L"too many counters");
		return;
	}


	InstanceManager::Options imo;
	imo.sortIndex = rain.read(L"SortIndex").asInt();
	imo.syncRawFormatted = rain.read(L"SyncRawFormatted").asBool();
	imo.keepDiscarded = rain.read(L"KeepDiscarded").asBool();
	imo.rollup = rain.read(L"Rollup").asBool();
	imo.limitIndexOffset = rain.read(L"LimitIndexOffset").asBool();

	auto str = rain.read(L"SortBy").asIString(L"None");
	typedef InstanceManager::SortBy SortBy;
	SortBy sortBy;
	if (str == L"None")
		sortBy = SortBy::eNONE;
	else if (str == L"InstanceName")
		sortBy = SortBy::eINSTANCE_NAME;
	else if (str == L"RawCounter")
		sortBy = SortBy::eRAW_COUNTER;
	else if (str == L"FormattedCounter")
		sortBy = SortBy::eFORMATTED_COUNTER;
	else if (str == L"Expression")
		sortBy = SortBy::eEXPRESSION;
	else if (str == L"RollupExpression")
		sortBy = SortBy::eROLLUP_EXPRESSION;
	else if (str == L"Count")
		sortBy = SortBy::eCOUNT;
	else {
		logger.error(L"SortBy '{}' is invalid, set to 'None'", str);
		sortBy = SortBy::eNONE;
	}
	imo.sortBy = sortBy;

	str = rain.read(L"SortOrder").asIString(L"Descending");
	typedef InstanceManager::SortOrder SortOrder;
	SortOrder sortOrder;
	if (str == L"Descending")
		sortOrder = SortOrder::eDESCENDING;
	else if (str == L"Ascending")
		sortOrder = SortOrder::eASCENDING;
	else {
		logger.error(L"SortOrder '{}' is invalid, set to 'Descending'", str);
		sortOrder = SortOrder::eDESCENDING;
	}
	imo.sortOrder = sortOrder;


	RollupFunction sortRollupFunction;
	auto rollupFunctionStr = rain.read(L"SortRollupFunction").asIString(L"Sum");
	if (rollupFunctionStr == L"Count") {
		logger.warning(L"SortRollupFunction 'Count' is deprecated, SortBy set to 'Count'");
		imo.sortBy = SortBy::eCOUNT;
		sortRollupFunction = RollupFunction::eSUM;
	} else {
		auto typeOpt = parseRollupFunction(rollupFunctionStr);
		if (typeOpt.has_value()) {
			sortRollupFunction = typeOpt.value();
		} else {
			logger.error(L"SortRollupFunction '{}' is invalid, set to 'Sum'", str);
			sortRollupFunction = RollupFunction::eSUM;
		}
	}
	imo.sortRollupFunction = sortRollupFunction;

	blacklistManager.setLists(
		rain.read(L"Blacklist").asString(),
		rain.read(L"BlacklistOrig").asString(),
		rain.read(L"Whitelist").asString(),
		rain.read(L"WhitelistOrig").asString()
	);

	const auto expressionTokens = rain.read(L"ExpressionList").asList(L'|');
	const auto rollupExpressionTokens = rain.read(L"RollupExpressionList").asList(L'|');
	expressionResolver.setExpressions(expressionTokens, rollupExpressionTokens);

	instanceManager.checkIndices(
		counterNames.size(),
		expressionResolver.getExpressionsCount(),
		expressionResolver.getRollupExpressionsCount()
	);


	typedef pdh::NamesManager::ModificationType NMT;
	NMT nameModificationType;
	bool needToFetchProcessIds = false;
	if (objectName == L"GPU Engine" || objectName == L"GPU Process Memory") {

		const auto displayName = rain.read(L"DisplayName").asIString(L"Original");
		if (displayName == L"Original") {
			nameModificationType = NMT::NONE;
		} else if (displayName == L"ProcessName" || displayName == L"GpuProcessName") {
			nameModificationType = NMT::GPU_PROCESS;
			needToFetchProcessIds = true;
		} else if (displayName == L"EngType" || displayName == L"GpuEngType") {
			nameModificationType = NMT::GPU_ENGTYPE;
		} else {
			logger.error(L"Object '{}' don't support DisplayName '{}', set to 'Original'", objectName, displayName);
			nameModificationType = NMT::NONE;
		}

	} else if (objectName == L"LogicalDisk") {

		const auto displayName = rain.read(L"DisplayName").asIString(L"Original");
		if (displayName == L"Original") {
			nameModificationType = NMT::NONE;
		} else if (displayName == L"DriveLetter") {
			nameModificationType = NMT::LOGICAL_DISK_DRIVE_LETTER;
		} else if (displayName == L"MountFolder") {
			nameModificationType = NMT::LOGICAL_DISK_MOUNT_PATH;
		} else {
			logger.error(L"Object '{}' don't support DisplayName '{}', set to 'Original'", objectName, displayName);
			nameModificationType = NMT::NONE;
		}

	} else if (objectName == L"Process") {
		nameModificationType = NMT::PROCESS;
	} else if (objectName == L"Thread") {
		nameModificationType = NMT::THREAD;
	} else {
		nameModificationType = NMT::NONE;
	}

	bool success = pdhWrapper.setCounters(objectName, counterNames, needToFetchProcessIds);
	if (!success) {
		setInvalid();
		return;
	}

	instanceManager.setOptions(imo);
	instanceManager.setNameModificationType(nameModificationType);
}

void PerfmonParent::getInstanceName(const InstanceInfo& instance, ResultString stringType, string& str) const {
	if (stringType == ResultString::eNUMBER) {
		return;
	}
	const auto& item = instanceManager.getNames(instance.indices.current);
	if (instanceManager.isRollup()) {
		str = item.displayName;
		return;
	}
	switch (stringType) {
	case ResultString::eORIGINAL_NAME:
		str = item.originalName;
		return;
	case ResultString::eUNIQUE_NAME: {
		auto ids = instanceManager.getIds(instance.indices.current);
		str = std::to_wstring(ids.id1);
		str += std::to_wstring(ids.id2);
		return;
	}
	case ResultString::eDISPLAY_NAME:
		str = item.displayName;
		return;
	case ResultString::eNUMBER:
		// just to shut up clang diagnostic
		return;
	}
}

double PerfmonParent::vUpdate() {

	if (!stopped) {
		const bool success = pdhWrapper.fetch();
		if (!success) {
			instanceManager.clear();
			state = State::eFETCH_ERROR;
			return 0;
		}

		instanceManager.swapSnapshot(pdhWrapper.getMainSnapshot(), pdhWrapper.getProcessIdsSnapshot());

		needUpdate = true;
	}

	if (!instanceManager.canGetRaw()) {
		state = State::eNO_DATA;
		return 0;
	}

	if (needUpdate) {
		// fetch or reload happened
		needUpdate = false;

		expressionResolver.resetCaches();

		instanceManager.update();
		instanceManager.sort(expressionResolver);
	}

	state = State::eOK;
	return 1;
}

void PerfmonParent::vUpdateString(string& resultStringBuffer) {
	switch (state) {
	case State::eFETCH_ERROR:
		resultStringBuffer = L"fetch error";
		break;
	case State::eNO_DATA:
		resultStringBuffer = L"no data";
		break;
	case State::eOK:
		resultStringBuffer = L"ok";
		break;
	default: std::terminate();
	}
}

void PerfmonParent::vCommand(isview bangArgs) {
	if (bangArgs == L"Stop") {
		stopped = true;
		return;
	}
	if (bangArgs == L"Resume") {
		stopped = false;
		return;
	}
	if (bangArgs == L"StopResume") {
		stopped = !stopped;
		return;
	}
	auto [name, value] = utils::Option{ bangArgs }.breakFirst(L' ');
	if (name.asIString() == L"SetIndexOffset") {
		const index offset = value.asInt();
		const auto firstSymbol = value.asString()[0];
		instanceManager.setIndexOffset(offset, firstSymbol == L'-' || firstSymbol == L'+');
	}
}

void PerfmonParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		return;
	}

	if (args[0] == L"fetch size") {
		resolveBufferString = std::to_wstring(instanceManager.getItemsCount());
		return;
	}
	if (args[0] == L"is stopped") {
		resolveBufferString = stopped ? L"1" : L"0";
		return;
	}
}
