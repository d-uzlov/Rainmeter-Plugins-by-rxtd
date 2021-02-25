/* 
 * Copyright (C) 2018-2021 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PerfmonParent.h"
#include "rxtd/option_parsing/OptionList.h"

using rxtd::perfmon::PerfmonParent;

PerfmonParent::PerfmonParent(Rainmeter&& _rain) : ParentMeasureBase(std::move(_rain)) {
	setUseResultString(true);

	bool success = pdhWrapper.init(logger);
	if (!success) {
		throw std::runtime_error{ "" };
	}

	const auto InstanceIndexOffset = rain.read(L"InstanceIndexOffset").asInt();
	simpleInstanceManager.setIndexOffset(InstanceIndexOffset, false);
	rollupInstanceManager.setIndexOffset(InstanceIndexOffset, false);
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

	useRollup = rain.read(L"Rollup").asBool();

	SimpleInstanceManager::Options imo;
	imo.syncRawFormatted = rain.read(L"SyncRawFormatted").asBool();
	imo.keepDiscarded = rain.read(L"KeepDiscarded").asBool();
	imo.limitIndexOffset = rain.read(L"LimitIndexOffset").asBool();

	imo.sortInfo = parseSortInfo();

	imo.blacklist = rain.read(L"Blacklist").asString();
	imo.blacklistOrig = rain.read(L"BlacklistOrig").asString();
	imo.whitelist = rain.read(L"Whitelist").asString();
	imo.whitelistOrig = rain.read(L"WhitelistOrig").asString();

	const auto expressionTokens = rain.read(L"ExpressionList").asList(L'|');
	expressionResolver.setExpressions(expressionTokens);

	const auto rollupExpressionTokens = rain.read(L"RollupExpressionList").asList(L'|');
	rollupExpressionSolver.setExpressions(rollupExpressionTokens);

	checkAndFixSortInfo(
		imo.sortInfo, 
		counterNames.size(),
		expressionResolver.getExpressionsCount(),
		rollupExpressionSolver.getExpressionsCount()
	);


	using NMT = pdh::NamesManager::ModificationType;
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

	simpleInstanceManager.setOptions(imo);
	simpleInstanceManager.setNameModificationType(nameModificationType);

	RollupInstanceManager::Options rio;
	rio.sortInfo = imo.sortInfo;
	rio.limitIndexOffset = imo.limitIndexOffset;
	rollupInstanceManager.setOptions(rio);
}

void PerfmonParent::getInstanceName(SimpleInstanceManager::Indices indices, ResultString stringType, string& str) const {
	if (stringType == ResultString::eNUMBER) {
		return;
	}
	const auto& item = simpleInstanceManager.getNames(indices.current);
	if (useRollup) {
		str = item.displayName;
		return;
	}
	switch (stringType) {
	case ResultString::eORIGINAL_NAME:
		str = item.originalName;
		return;
	case ResultString::eUNIQUE_NAME: {
		auto ids = simpleInstanceManager.getIds(indices.current);
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
			simpleInstanceManager.clear();
			state = State::eFETCH_ERROR;
			return 0;
		}

		simpleInstanceManager.swapSnapshot(pdhWrapper.getMainSnapshot(), pdhWrapper.getProcessIdsSnapshot());

		needUpdate = true;
	}

	if (!simpleInstanceManager.canGetRaw()) {
		state = State::eNO_DATA;
		return 0;
	}

	if (needUpdate) {
		// fetch or reload happened
		needUpdate = false;

		expressionResolver.resetCache();

		simpleInstanceManager.update();
		if (useRollup) {
			rollupInstanceManager.update();
			rollupInstanceManager.sort(rollupExpressionSolver);
		} else {
			simpleInstanceManager.sort(expressionResolver);
		}
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
	auto [name, value] = option_parsing::Option{ bangArgs }.breakFirst(L' ');
	if (name.asIString() == L"SetIndexOffset") {
		const index offset = value.asInt();
		const auto firstSymbol = value.asString()[0];
		const bool isRelativeValue = firstSymbol == L'-' || firstSymbol == L'+';
		simpleInstanceManager.setIndexOffset(offset, isRelativeValue);
		rollupInstanceManager.setIndexOffset(offset, isRelativeValue);
	}
}

void PerfmonParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		return;
	}

	if (args[0] == L"fetch size") {
		resolveBufferString = std::to_wstring(simpleInstanceManager.getItemsCount());
		return;
	}
	if (args[0] == L"is stopped") {
		resolveBufferString = stopped ? L"1" : L"0";
		return;
	}
}

double PerfmonParent::getValues(const Reference& ref, index sortedIndex, ResultString stringType, string& str) const {
	if (!simpleInstanceManager.canGetRaw() || ref.type == Reference::Type::eCOUNTER_FORMATTED && !simpleInstanceManager.canGetFormatted()) {
		str = ref.total ? L"Total" : L"";
		return 0.0;
	}

	if (ref.total) {
		str = L"Total";
		if (useRollup) {
			return rollupExpressionSolver.resolveReference(ref, {});
		} else {
			return expressionResolver.resolveReference(ref, {});
		}
	}

	if (useRollup) {
		const auto instance = rollupInstanceManager.findRollupInstance(ref, sortedIndex);
		str.clear();
		getInstanceName(instance->indices.front(), stringType, str);
		return rollupExpressionSolver.resolveReference(ref, instance->indices);
	}

	const auto instance = simpleInstanceManager.findSimpleInstance(ref, sortedIndex);
	str.clear();
	getInstanceName(instance->indices, stringType, str);
	return expressionResolver.resolveReference(ref, instance->indices);
}

rxtd::perfmon::SortInfo PerfmonParent::parseSortInfo() {
	SortInfo result;

	result.sortByValueInformation.sortIndex = rain.read(L"SortIndex").asInt();

	const auto sortByString = rain.read(L"SortBy").asIString(L"None");
	if (const auto sortByOpt = parseEnum<SortBy>(sortByString);
		sortByOpt.has_value()) {
		result.sortBy = sortByOpt.value();
	} else {
		result.sortBy = SortBy::eVALUE;

		if (sortByString == L"RawCounter")
			result.sortByValueInformation.expressionType = Reference::Type::eCOUNTER_RAW;
		else if (sortByString == L"FormattedCounter")
			result.sortByValueInformation.expressionType = Reference::Type::eCOUNTER_FORMATTED;
		else if (sortByString == L"Expression")
			result.sortByValueInformation.expressionType = Reference::Type::eEXPRESSION;
		else if (sortByString == L"RollupExpression")
			result.sortByValueInformation.expressionType = Reference::Type::eROLLUP_EXPRESSION;
		else if (sortByString == L"Count")
			result.sortByValueInformation.expressionType = Reference::Type::eCOUNT;
		else {
			logger.error(L"SortBy '{}' is invalid, set to 'None'", sortByString);
			result.sortBy = SortBy::eNONE;
		}
	}

	const auto sortOrderString = rain.read(L"SortOrder").asIString(L"Descending");
	if (const auto sortOrderOpt = parseEnum<SortOrder>(sortOrderString);
		sortOrderOpt.has_value()) {
		result.sortOrder = sortOrderOpt.value();
	} else {
		logger.error(L"SortOrder '{}' is invalid, set to 'Descending'", sortOrderString);
		result.sortOrder = SortOrder::eDESCENDING;
	}

	if (const auto rollupFunctionStr = rain.read(L"SortRollupFunction").asIString(L"Sum");
		rollupFunctionStr == L"Count") {
		logger.warning(L"SortRollupFunction 'Count' is deprecated, SortBy set to 'Count'");
		result.sortBy = SortBy::eVALUE;
		result.sortByValueInformation.expressionType = Reference::Type::eCOUNT;
		result.sortByValueInformation.sortRollupFunction = RollupFunction::eSUM;
	} else {
		auto typeOpt = parseEnum<RollupFunction>(rollupFunctionStr);
		if (typeOpt.has_value()) {
			result.sortByValueInformation.sortRollupFunction = typeOpt.value();
		} else {
			logger.error(L"SortRollupFunction '{}' is invalid, set to 'Sum'", rollupFunctionStr);
			result.sortByValueInformation.sortRollupFunction = RollupFunction::eSUM;
		}
	}

	return result;
}

void PerfmonParent::checkAndFixSortInfo(SortInfo& sortInfo, index counters, index expressions, index rollupExpressions) const {
	index checkCount = 0;

	if (sortInfo.sortBy != SortBy::eVALUE) {
		return;
	}

	auto info = sortInfo.sortByValueInformation;

	if (info.sortIndex < 0) {
		logger.error(L"SortIndex can't be negative");
		sortInfo.sortBy = SortBy::eNONE;
		return;
	}

	switch (info.expressionType) {
	case Reference::Type::eCOUNTER_RAW:
	case Reference::Type::eCOUNTER_FORMATTED:
		checkCount = counters;
		break;
	case Reference::Type::eEXPRESSION: {
		if (expressions <= 0) {
			logger.error(L"Sort by Expression requires at least 1 Expression specified. Set to None.");
			sortInfo.sortBy = SortBy::eNONE;
			return;
		}
		checkCount = expressions;
		break;
	}
	case Reference::Type::eROLLUP_EXPRESSION: {
		if (!useRollup) {
			logger.error(L"RollupExpressions can't be used for sort if rollup is disabled. Set to None.");
			sortInfo.sortBy = SortBy::eNONE;
			return;
		}
		if (rollupExpressions <= 0) {
			logger.error(L"Sort by RollupExpression requires at least 1 RollupExpression specified. Set to None.");
			sortInfo.sortBy = SortBy::eNONE;
			return;
		}
		checkCount = rollupExpressions;
		break;
	}
	case Reference::Type::eCOUNT: {
		if (!useRollup) {
			logger.warning(L"SortBy Count does nothing when rollup is disabled");
			sortInfo.sortBy = SortBy::eNONE;
		}
		return;
	}
	}

	if (info.sortIndex >= checkCount) {
		logger.error(L"SortIndex {} is out of bounds (must be in [0; {}]). Set to 0.", checkCount, info.sortIndex);
		info.sortIndex = 0;
	}
}
