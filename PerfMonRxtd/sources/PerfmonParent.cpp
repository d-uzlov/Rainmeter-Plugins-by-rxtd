/* 
 * Copyright (C) 2018-2019 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <functional>
#include <unordered_map>

#include "expressions.h"
#include "PerfmonParent.h"
#include "OptionParser.h"

#include "undef.h"

using namespace perfmon;

utils::ParentManager<PerfmonParent> PerfmonParent::parentManager { };

PerfmonParent::~PerfmonParent() {
	parentManager.remove(*this);
}

PerfmonParent::PerfmonParent(utils::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	parentManager.add(*this);

	objectName = rain.readString(L"ObjectName");
	instanceManager.setIndexOffset(rain.read(L"InstanceIndexOffset").asInt());


	if (objectName.empty()) {
		logger.error(L"ObjectName must be specified");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	auto counterTokens = rain.read(L"CounterList").asList(L'|').own();

	if (counterTokens.empty()) {
		logger.error(L"CounterList must have at least one entry");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	pdhWrapper = pdh::PdhWrapper { logger, objectName, counterTokens };
	if (!pdhWrapper.isValid()) {
		setMeasureState(utils::MeasureState::eBROKEN);
	}

}

PerfmonParent* PerfmonParent::findInstance(utils::Rainmeter::Skin skin, isview measureName) {
	return parentManager.findParent(skin, measureName);
}

void PerfmonParent::_reload() {
	needUpdate = true;

	instanceManager.setSortIndex(rain.read(L"SortIndex").asInt<counter_t>());
	instanceManager.setSyncRawFormatted(rain.read(L"SyncRawFormatted").asBool());
	instanceManager.setKeepDiscarded(rain.read(L"KeepDiscarded").asBool());

	instanceManager.setRollup(rain.read(L"Rollup").asBool());
	instanceManager.setLimitIndexOffset(rain.read(L"LimitIndexOffset").asBool());


	auto str = rain.readString(L"SortBy") % ciView();
	typedef InstanceManager::SortBy SortBy;
	SortBy sortBy;
	if (str.empty() || str== L"None")
		sortBy = SortBy::eNONE;
	else if (str==L"InstanceName")
		sortBy = SortBy::eINSTANCE_NAME;
	else if (str==L"RawCounter")
		sortBy = SortBy::eRAW_COUNTER;
	else if (str==L"FormattedCounter")
		sortBy = SortBy::eFORMATTED_COUNTER;
	else if (str==L"Expression")
		sortBy = SortBy::eEXPRESSION;
	else if (str==L"RollupExpression")
		sortBy = SortBy::eROLLUP_EXPRESSION;
	else if (str==L"Count")
		sortBy = SortBy::eCOUNT;
	else {
		logger.error(L"SortBy '{}' is invalid, set to 'None'", str);
		sortBy = SortBy::eNONE;
	}
	instanceManager.setSortBy(sortBy);

	str = rain.readString(L"SortOrder") % ciView();
	typedef InstanceManager::SortOrder SortOrder;
	SortOrder sortOrder;
	if (str==L"" || str==L"Descending")
		sortOrder = SortOrder::eDESCENDING;
	else if (str==L"Ascending")
		sortOrder = SortOrder::eASCENDING;
	else {
		logger.error(L"SortOrder '{}' is invalid, set to 'Descending'", str);
		sortOrder = SortOrder::eDESCENDING;
	}
	instanceManager.setSortOrder(sortOrder);

	str = rain.readString(L"SortRollupFunction") % ciView();
	RollupFunction sortRollupFunction;
	if (str==L"" || str==L"Sum")
		sortRollupFunction = RollupFunction::eSUM;
	else if (str==L"Average")
		sortRollupFunction = RollupFunction::eAVERAGE;
	else if (str==L"Minimum")
		sortRollupFunction = RollupFunction::eMINIMUM;
	else if (str==L"Maximum")
		sortRollupFunction = RollupFunction::eMAXIMUM;
	else if (str==L"Count") {
		logger.warning(L"SortRollupFunction 'Count' is deprecated, SortBy set to 'Count'");
		instanceManager.setSortBy(SortBy::eCOUNT);
		sortRollupFunction = RollupFunction::eSUM;
	} else {
		logger.error(L"SortRollupFunction '{}' is invalid, set to 'Sum'", str);
		sortRollupFunction = RollupFunction::eSUM;
	}
	instanceManager.setSortRollupFunction(sortRollupFunction);

	blacklistManager.setLists(
		rain.readString(L"Blacklist") % own(),
		rain.readString(L"BlacklistOrig") % own(),
		rain.readString(L"Whitelist") % own(),
		rain.readString(L"WhitelistOrig") % own()
	);

	const auto expressionTokens = rain.read(L"ExpressionList").asList(L'|');
	const auto rollupExpressionTokens = rain.read(L"RollupExpressionList").asList(L'|');
	expressionResolver.setExpressions(expressionTokens, rollupExpressionTokens);

	instanceManager.checkIndices(
		pdhWrapper.getCountersCount(),
		expressionResolver.getExpressionsCount(),
		expressionResolver.getRollupExpressionsCount()
	);


	typedef pdh::NamesManager::ModificationType NMT;
	NMT nameModificationType;
	if (objectName == L"GPU Engine" || objectName == L"GPU Process Memory") {

		const auto displayName = rain.readString(L"DisplayName") % ciView();
		if (displayName==L"" || displayName==L"Original") {
			nameModificationType = NMT::NONE;
		} else if (displayName==L"ProcessName" || displayName==L"GpuProcessName") {
			nameModificationType = NMT::GPU_PROCESS;
		} else if (displayName==L"EngType" || displayName==L"GpuEngType") {
			nameModificationType = NMT::GPU_ENGTYPE;
		} else {
			logger.error(L"Object '{}' don't support DisplayName '{}', set to 'Original'", objectName, displayName);
			nameModificationType = NMT::NONE;
		}

	} else if (objectName == L"LogicalDisk") {

		const auto displayName = rain.readString(L"DisplayName") % ciView();
		if (displayName==L"" || displayName==L"Original") {
			nameModificationType = NMT::NONE;
		} else if (displayName==L"DriveLetter") {
			nameModificationType = NMT::LOGICAL_DISK_DRIVE_LETTER;
		} else if (displayName==L"MountFolder") {
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

	instanceManager.setNameModificationType(nameModificationType);
}

const InstanceInfo* PerfmonParent::findInstance(const Reference& ref, item_t sortedIndex) const {
	return instanceManager.findInstance(ref, sortedIndex);
}

sview PerfmonParent::getInstanceName(const InstanceInfo& instance, ResultString stringType) const {
	if (stringType == ResultString::eNUMBER) {
		return L"";
	}
	const auto& item = instanceManager.getNames(instance.indices.current);
	if (instanceManager.isRollup()) {
		return item.displayName;
	}
	switch (stringType) {
	case ResultString::eORIGINAL_NAME:
		return item.originalName;
	case ResultString::eUNIQUE_NAME:
		return item.uniqueName;
	case ResultString::eDISPLAY_NAME:
		return item.displayName;
	default:
		logger.error(L"unexpected result string type {}", stringType);
		return L"";
	}
}

std::tuple<double, const wchar_t*> PerfmonParent::_update() {

	if (!stopped) {
		std::swap(snapshotCurrent, snapshotPrevious);

		const bool success = pdhWrapper.fetch(snapshotCurrent, idSnapshot);
		if (!success) {
			snapshotCurrent.clear();
			snapshotPrevious.clear();
			return std::make_tuple(0, L"fetch error");
		}

		needUpdate = true;
	}

	if (!canGetRaw()) {
		return std::make_tuple(0, L"no data");
	}

	if (needUpdate) { // fetch or reload happened
		needUpdate = false;

		expressionResolver.resetCaches();

		instanceManager.update();
		instanceManager.sort(expressionResolver);
	}

	return std::make_tuple(1, L"ok");
}

void PerfmonParent::_command(isview bangArgs) {
	if (bangArgs== L"Stop") {
		setStopped(true);
		return;
	}
	if (bangArgs==L"Resume") {
		setStopped(false);
		return;
	}
	if (bangArgs== L"StopResume") {
		changeStopState();
		return;
	}
	if (bangArgs.substr(0, 14)== L"SetIndexOffset") {
		auto arg = bangArgs.substr(14);
		arg = utils::StringUtils::trim(arg);
		if (arg[0] == L'-' || arg[0] == L'+') {
			const auto offset = getIndexOffset() + utils::StringUtils::parseInt(arg % csView());
			setIndexOffset(offset);
		} else {
			const auto offset = utils::StringUtils::parseInt(arg % csView());
			setIndexOffset(offset);
		}
	}
}

const wchar_t* PerfmonParent::_resolve(int argc, const wchar_t* argv[]) {
	if (argc < 1) {
		return nullptr;
	}

	bufferString = argv[0];
	utils::StringUtils::trimInplace(bufferString);
	utils::StringUtils::lowerInplace(bufferString);
	if (bufferString == L"fetch size") {
		bufferString = std::to_wstring(snapshotCurrent.getItemsCount());
		return bufferString.c_str();
	}
	if (bufferString == L"is stopped") {
		return stopped ? L"1" : L"0";
	}

	return nullptr;
}

void PerfmonParent::setStopped(const bool value) {
	stopped = value;
}
void PerfmonParent::changeStopState() {
	stopped = !stopped;
}
void PerfmonParent::setIndexOffset(item_t value) {
	instanceManager.setIndexOffset(value);
}
item_t PerfmonParent::getIndexOffset() const {
	return instanceManager.getIndexOffset();
}

double PerfmonParent::getValue(const Reference& ref, const InstanceInfo* instance, utils::Rainmeter::Logger& logger) const {
	return expressionResolver.getValue(ref, instance, logger);
}

counter_t PerfmonParent::getCountersCount() const {
	return pdhWrapper.getCountersCount();
}

bool PerfmonParent::canGetRaw() const {
	return instanceManager.canGetRaw();
}

bool PerfmonParent::canGetFormatted() const {
	return instanceManager.canGetFormatted();
}

