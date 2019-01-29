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
	instanceManager.setIndexOffset(rain.readInt(L"InstanceIndexOffset"));


	if (objectName.empty()) {
		log.error(L"ObjectName must be specified");
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}

	utils::OptionParser optionParser;
	auto counterTokens = optionParser.asList(rain.readString(L"CounterList"), L'|');

	if (counterTokens.empty()) {
		log.error(L"CounterList must have at least one entry");
		setMeasureState(utils::MeasureState::BROKEN);
		return;
	}

	pdhWrapper = pdh::PdhWrapper { log, objectName, counterTokens };
	if (!pdhWrapper.isValid()) {
		setMeasureState(utils::MeasureState::BROKEN);
	}

}

PerfmonParent* PerfmonParent::findInstance(utils::Rainmeter::Skin skin, isview measureName) {
	return parentManager.findParent(skin, measureName);
}

void PerfmonParent::_reload() {
	if (getState() == utils::MeasureState::BROKEN) {
		return;
	}

	needUpdate = true;

	instanceManager.setSortIndex(rain.readInt<counter_t>(L"SortIndex"));
	instanceManager.setSyncRawFormatted(rain.readBool(L"SyncRawFormatted"));
	instanceManager.setKeepDiscarded(rain.readBool(L"KeepDiscarded"));

	instanceManager.setRollup(rain.readBool(L"Rollup"));
	instanceManager.setLimitIndexOffset(rain.readBool(L"LimitIndexOffset"));


	auto str = rain.readString(L"SortBy") % ciView();
	typedef InstanceManager::SortBy SortBy;
	SortBy sortBy;
	if (str.empty() || str== L"None")
		sortBy = SortBy::NONE;
	else if (str==L"InstanceName")
		sortBy = SortBy::INSTANCE_NAME;
	else if (str==L"RawCounter")
		sortBy = SortBy::RAW_COUNTER;
	else if (str==L"FormattedCounter")
		sortBy = SortBy::FORMATTED_COUNTER;
	else if (str==L"Expression")
		sortBy = SortBy::EXPRESSION;
	else if (str==L"RollupExpression")
		sortBy = SortBy::ROLLUP_EXPRESSION;
	else if (str==L"Count")
		sortBy = SortBy::COUNT;
	else {
		log.error(L"SortBy '{}' is invalid, set to 'None'", str);
		sortBy = SortBy::NONE;
	}
	instanceManager.setSortBy(sortBy);

	str = rain.readString(L"SortOrder") % ciView();
	typedef InstanceManager::SortOrder SortOrder;
	SortOrder sortOrder;
	if (str==L"" || str==L"Descending")
		sortOrder = SortOrder::DESCENDING;
	else if (str==L"Ascending")
		sortOrder = SortOrder::ASCENDING;
	else {
		log.error(L"SortOrder '{}' is invalid, set to 'Descending'", str);
		sortOrder = SortOrder::DESCENDING;
	}
	instanceManager.setSortOrder(sortOrder);

	str = rain.readString(L"SortRollupFunction") % ciView();
	RollupFunction sortRollupFunction;
	if (str==L"" || str==L"Sum")
		sortRollupFunction = RollupFunction::SUM;
	else if (str==L"Average")
		sortRollupFunction = RollupFunction::AVERAGE;
	else if (str==L"Minimum")
		sortRollupFunction = RollupFunction::MINIMUM;
	else if (str==L"Maximum")
		sortRollupFunction = RollupFunction::MAXIMUM;
	else if (str==L"Count") {
		log.warning(L"SortRollupFunction 'Count' is deprecated, SortBy set to 'Count'");
		instanceManager.setSortBy(SortBy::COUNT);
		sortRollupFunction = RollupFunction::SUM;
	} else {
		log.error(L"SortRollupFunction '{}' is invalid, set to 'Sum'", str);
		sortRollupFunction = RollupFunction::SUM;
	}
	instanceManager.setSortRollupFunction(sortRollupFunction);

	blacklistManager.setLists(
		optionParser.asList(rain.readString(L"Blacklist"), L'|'),
		optionParser.asList(rain.readString(L"BlacklistOrig"), L'|'),
		optionParser.asList(rain.readString(L"Whitelist"), L'|'),
		optionParser.asList(rain.readString(L"WhitelistOrig"), L'|')
	);

	utils::OptionParser optionParser;

	const auto expressionTokens = optionParser.asList(rain.readString(L"ExpressionList"), L'|');
	const auto rollupExpressionTokens = optionParser.asList(rain.readString(L"RollupExpressionList"), L'|');
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
			log.error(L"Object '{}' don't support DisplayName '{}', set to 'Original'", objectName, displayName);
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
			log.error(L"Object '{}' don't support DisplayName '{}', set to 'Original'", objectName, displayName);
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
	if (stringType == ResultString::NUMBER) {
		return L"";
	}
	const auto& item = instanceManager.getNames(instance.indices.current);
	if (instanceManager.isRollup()) {
		return item.displayName;
	}
	switch (stringType) {
	case ResultString::ORIGINAL_NAME:
		return item.originalName;
	case ResultString::UNIQUE_NAME:
		return item.uniqueName;
	case ResultString::DISPLAY_NAME:
		return item.displayName;
	default:
		log.error(L"unexpected result string type {}", stringType);
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

void PerfmonParent::_command(const wchar_t* bangArgsC) {
	isview bangArgs = bangArgsC;
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

