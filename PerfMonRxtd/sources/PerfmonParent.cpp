/* 
 * Copyright (C) 2018-2019 rxtd
 * Copyright (C) 2018 buckb
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <vector>
#include <algorithm>
#include <cwctype>
#include <functional>
#include <unordered_map>
#include <string>
#include <string_view>

#include "expressions.h"
#include "PerfmonParent.h"
#include "OptionParser.h"



#pragma warning(disable : 26451)

rxu::ParentManager<rxpm::PerfmonParent> rxpm::PerfmonParent::parentManager { };

rxpm::PerfmonParent::~PerfmonParent() {
	parentManager.remove(*this);
}

rxpm::PerfmonParent::PerfmonParent(rxu::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	parentManager.add(*this);

	objectName = rain.readString(L"ObjectName");
	instanceManager.setIndexOffset(rain.readInt(L"InstanceIndexOffset"));


	if (objectName.empty()) {
		log.error(L"ObjectName must be specified");
		setMeasureState(rxu::MeasureState::BROKEN);
		return;
	}

	rxu::OptionParser optionParser;
	auto counterTokens = optionParser.asList(rain.readString(L"CounterList"), L'|');

	if (counterTokens.empty()) {
		log.error(L"CounterList must have at least one entry");
		setMeasureState(rxu::MeasureState::BROKEN);
		return;
	}

	pdhWrapper = pdh::PdhWrapper { log, objectName, counterTokens };
	if (!pdhWrapper.isValid()) {
		setMeasureState(rxu::MeasureState::BROKEN);
	}

}

rxpm::PerfmonParent* rxpm::PerfmonParent::findInstance(rxu::Rainmeter::Skin skin, const wchar_t* measureName) {
	return parentManager.findParent(skin, measureName);
}

void rxpm::PerfmonParent::_reload() {
	if (getState() == rxu::MeasureState::BROKEN) {
		return;
	}

	needUpdate = true;

	instanceManager.setSortIndex(rain.readInt(L"SortIndex"));
	instanceManager.setSyncRawFormatted(rain.readBool(L"SyncRawFormatted"));
	instanceManager.setKeepDiscarded(rain.readBool(L"KeepDiscarded"));

	instanceManager.setRollup(rain.readBool(L"Rollup"));
	instanceManager.setLimitIndexOffset(rain.readBool(L"LimitIndexOffset"));


	auto str = rain.readString(L"SortBy");
	typedef InstanceManager::SortBy SortBy;
	SortBy sortBy;
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"None") == 0)
		sortBy = SortBy::NONE;
	else if (_wcsicmp(str, L"InstanceName") == 0)
		sortBy = SortBy::INSTANCE_NAME;
	else if (_wcsicmp(str, L"RawCounter") == 0)
		sortBy = SortBy::RAW_COUNTER;
	else if (_wcsicmp(str, L"FormattedCounter") == 0)
		sortBy = SortBy::FORMATTED_COUNTER;
	else if (_wcsicmp(str, L"Expression") == 0)
		sortBy = SortBy::EXPRESSION;
	else if (_wcsicmp(str, L"RollupExpression") == 0)
		sortBy = SortBy::ROLLUP_EXPRESSION;
	else if (_wcsicmp(str, L"Count") == 0)
		sortBy = SortBy::COUNT;
	else {
		log.error(L"SortBy '{}' is invalid, set to 'None'", str);
		sortBy = SortBy::NONE;
	}
	instanceManager.setSortBy(sortBy);

	str = rain.readString(L"SortOrder");
	typedef InstanceManager::SortOrder SortOrder;
	SortOrder sortOrder;
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Descending") == 0)
		sortOrder = SortOrder::DESCENDING;
	else if (_wcsicmp(str, L"Ascending") == 0)
		sortOrder = SortOrder::ASCENDING;
	else {
		log.error(L"SortOrder '{}' is invalid, set to 'Descending'", str);
		sortOrder = SortOrder::DESCENDING;
	}
	instanceManager.setSortOrder(sortOrder);

	str = rain.readString(L"SortRollupFunction");
	RollupFunction sortRollupFunction;
	if (_wcsicmp(str, L"") == 0 || _wcsicmp(str, L"Sum") == 0)
		sortRollupFunction = RollupFunction::SUM;
	else if (_wcsicmp(str, L"Average") == 0)
		sortRollupFunction = RollupFunction::AVERAGE;
	else if (_wcsicmp(str, L"Minimum") == 0)
		sortRollupFunction = RollupFunction::MINIMUM;
	else if (_wcsicmp(str, L"Maximum") == 0)
		sortRollupFunction = RollupFunction::MAXIMUM;
	else if (_wcsicmp(str, L"Count") == 0) {
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

	rxu::OptionParser optionParser;

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

		const wchar_t* displayName = rain.readString(L"DisplayName");
		if (_wcsicmp(displayName, L"") == 0 || _wcsicmp(displayName, L"Original") == 0) {
			nameModificationType = NMT::NONE;
		} else if (_wcsicmp(displayName, L"ProcessName") == 0 || _wcsicmp(displayName, L"GpuProcessName") == 0) {
			nameModificationType = NMT::GPU_PROCESS;
		} else if (_wcsicmp(displayName, L"EngType") == 0 || _wcsicmp(displayName, L"GpuEngType") == 0) {
			nameModificationType = NMT::GPU_ENGTYPE;
		} else {
			log.error(L"Object '{}' don't support DisplayName '{}', set to 'Original'", objectName, displayName);
			nameModificationType = NMT::NONE;
		}

	} else if (objectName == L"LogicalDisk") {

		const wchar_t* displayName = rain.readString(L"DisplayName");
		if (_wcsicmp(displayName, L"") == 0 || _wcsicmp(displayName, L"Original") == 0) {
			nameModificationType = NMT::NONE;
		} else if (_wcsicmp(displayName, L"DriveLetter") == 0) {
			nameModificationType = NMT::LOGICAL_DISK_DRIVE_LETTER;
		} else if (_wcsicmp(displayName, L"MountFolder") == 0) {
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

const rxpm::InstanceInfo* rxpm::PerfmonParent::findInstance(const Reference& ref, unsigned long sortedIndex) const {
	return instanceManager.findInstance(ref, sortedIndex);
}

std::wstring_view rxpm::PerfmonParent::getInstanceName(const InstanceInfo& instance, ResultString stringType) const {
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

std::tuple<double, const wchar_t*> rxpm::PerfmonParent::_update() {

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

void rxpm::PerfmonParent::_command(const wchar_t* bangArgs) {
	if (_wcsicmp(bangArgs, L"Stop") == 0) {
		setStopped(true);
		return;
	}
	if (_wcsicmp(bangArgs, L"Resume") == 0) {
		setStopped(false);
		return;
	}
	if (_wcsicmp(bangArgs, L"StopResume") == 0) {
		changeStopState();
		return;
	}
	std::wstring str = bangArgs;
	if (_wcsicmp(str.substr(0, 14).c_str(), L"SetIndexOffset") == 0) {
		std::wstring arg = str.substr(14);
		arg = rxu::StringUtils::trimCopy(arg);
		if (arg[0] == L'-' || arg[0] == L'+') {
			const int offset = getIndexOffset() + _wtoi(arg.c_str());
			setIndexOffset(offset);
		} else {
			const int offset = _wtoi(arg.c_str());
			setIndexOffset(offset);
		}
	}
}

const wchar_t* rxpm::PerfmonParent::_resolve(int argc, const wchar_t* argv[]) {
	if (argc < 1) {
		return nullptr;
	}

	bufferString = argv[0];
	rxu::StringUtils::trimInplace(bufferString);
	rxu::StringUtils::lowerInplace(bufferString);
	if (bufferString == L"fetch size") {
		bufferString = std::to_wstring(snapshotCurrent.getItemsCount());
		return bufferString.c_str();
	}
	if (bufferString == L"is stopped") {
		return stopped ? L"1" : L"0";
	}

	return nullptr;
}

void rxpm::PerfmonParent::setStopped(const bool value) {
	stopped = value;
}
void rxpm::PerfmonParent::changeStopState() {
	stopped = !stopped;
}
void rxpm::PerfmonParent::setIndexOffset(int value) {
	instanceManager.setIndexOffset(value);
}
int rxpm::PerfmonParent::getIndexOffset() const {
	return instanceManager.getIndexOffset();
}

double rxpm::PerfmonParent::getValue(const Reference& ref, const InstanceInfo* instance, rxu::Rainmeter::Logger& logger) const {
	return expressionResolver.getValue(ref, instance, logger);
}

size_t rxpm::PerfmonParent::getCountersCount() const {
	return pdhWrapper.getCountersCount();
}

bool rxpm::PerfmonParent::canGetRaw() const {
	return instanceManager.canGetRaw();
}

bool rxpm::PerfmonParent::canGetFormatted() const {
	return instanceManager.canGetFormatted();
}
