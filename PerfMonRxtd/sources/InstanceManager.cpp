/*
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "InstanceManager.h"
#include "ExpressionResolver.h"

#include "undef.h"

using namespace perfmon;

InstanceManager::InstanceManager(
	utils::Rainmeter::Logger& log,
	const pdh::PdhWrapper& phWrapper,
	const pdh::Snapshot& idSnapshot,
	const pdh::Snapshot& snapshotCurrent, const pdh::Snapshot& snapshotPrevious,
	const BlacklistManager& blacklistManager) :
	log(log),
	pdhWrapper(phWrapper),
	idSnapshot(idSnapshot),
	snapshotCurrent(snapshotCurrent),
	snapshotPrevious(snapshotPrevious),
	blacklistManager(blacklistManager) { }

void InstanceManager::setSyncRawFormatted(bool value) {
	syncRawFormatted = value;
}

void InstanceManager::setRollup(bool value) {
	rollup = value;
}

void InstanceManager::setIndexOffset(index value) {
	indexOffset = value;
	if (limitIndexOffset && indexOffset < 0) {
		indexOffset = 0;
	}
}

void InstanceManager::setLimitIndexOffset(bool value) {
	limitIndexOffset = value;
	if (limitIndexOffset && indexOffset < 0) {
		indexOffset = 0;
	}
}

index InstanceManager::getIndexOffset() const {
	return indexOffset;
}

bool InstanceManager::isRollup() const {
	return rollup;
}

index InstanceManager::getCountersCount() const {
	return pdhWrapper.getCountersCount();
}

const pdh::ModifiedNameItem& InstanceManager::getNames(index index) const {
	return namesCurrent.get(index);
}

void InstanceManager::setSortIndex(index value) {
	if (value >= 0) {
		sortIndex = value;
		return;
	}

	log.error(L"SortIndex must be >= 0, but {} found, set to 0", value);
	sortIndex = 0;
}

void InstanceManager::setKeepDiscarded(bool value) {
	keepDiscarded = value;
}

void InstanceManager::setSortBy(SortBy value) {
	sortBy = value;
}

void InstanceManager::setSortOrder(SortOrder value) {
	sortOrder = value;
}

void InstanceManager::setSortRollupFunction(RollupFunction value) {
	sortRollupFunction = value;
}

void InstanceManager::checkIndices(index counters, index expressions, index rollupExpressions) {
	if (sortBy == SortBy::EXPRESSION) {
		if (expressions <= 0) {
			log.error(L"Sort by Expression requires at least 1 Expression specified. Set to None.");
			sortBy = SortBy::NONE;
			return;
		}
	}
	if (sortBy == SortBy::ROLLUP_EXPRESSION) {
		if (!rollupExpressions) {
			log.error(L"RollupExpressions can't be used for sort if rollup is disabled. Set to None.");
			sortBy = SortBy::NONE;
			return;
		}
		if (rollupExpressions <= 0) {
			log.error(L"Sort by RollupExpression requires at least 1 RollupExpression specified. Set to None.");
			sortBy = SortBy::NONE;
			return;
		}
	}

	index checkCount;
	switch (sortBy) {
	case SortBy::NONE: return;
	case SortBy::INSTANCE_NAME: return;
	case SortBy::RAW_COUNTER: [[fallthrough]];
	case SortBy::FORMATTED_COUNTER:
		checkCount = counters;
		break;
	case SortBy::EXPRESSION:
		checkCount = expressions;
		break;
	case SortBy::ROLLUP_EXPRESSION:
		checkCount = rollupExpressions;
		break;
	case SortBy::COUNT: return;
	default: std::terminate();
	}

	if (sortIndex < checkCount) {
		return;
	}
	log.error(L"SortIndex must be in [0; {}], but {} found. Set to 0.", expressions, sortIndex);
	sortIndex = 0;
	return;
}

const std::vector<InstanceInfo>& InstanceManager::getInstances() const {
	return instances;
}

const std::vector<InstanceInfo>& InstanceManager::getDiscarded() const {
	return instancesDiscarded;
}

const std::vector<InstanceInfo>& InstanceManager::getRollupInstances() const {
	return instancesRolledUp;
}

bool InstanceManager::canGetRaw() const {
	return !snapshotCurrent.isEmpty() && (!syncRawFormatted || !snapshotPrevious.isEmpty());
}

bool InstanceManager::canGetFormatted() const {
	return !snapshotCurrent.isEmpty() && !snapshotPrevious.isEmpty();
}

void InstanceManager::setNameModificationType(pdh::NamesManager::ModificationType value) {
	namesCurrent.setModificationType(value);
	namesPrevious.setModificationType(value);
}

void InstanceManager::update() {
	instances.clear();
	instancesRolledUp.clear();
	instancesDiscarded.clear();

	nameCache.clear();
	nameCacheRollup.clear();
	nameCacheDiscarded.clear();

	if (snapshotCurrent.isEmpty()) {
		return;
	}

	std::swap(namesCurrent, namesPrevious);
	namesCurrent.createModifiedNames(snapshotCurrent, idSnapshot);

	if (snapshotPrevious.isEmpty()) {
		buildInstanceKeysZero();
	} else {
		buildInstanceKeys();
	}
	if (rollup) {
		buildRollupKeys();
	}
}

index InstanceManager::findPreviousName(sview uniqueName, index hint) const {
	// try to find a match for the current instance name in the previous names buffer
	// use the unique name for this search because we need a unique match
	// counter buffers tend to be *mostly* aligned, so we'll try to short-circuit a full search

	const auto itemCountPrevious = snapshotPrevious.getItemsCount();

	// try for a direct hit
	auto previousInx = std::clamp<index>(hint, 0, itemCountPrevious - 1);

	if (uniqueName == namesPrevious.get(previousInx).uniqueName) {
		return previousInx;
	}

	// try a window around currentIndex
	constexpr index windowSize = 5;

	const auto lowBound = std::clamp<index>(hint - windowSize, 0, itemCountPrevious - 1);
	const auto highBound = std::clamp<index>(hint + windowSize, 0, itemCountPrevious - 1);

	for (previousInx = lowBound; previousInx <= highBound; ++previousInx) {
		if (uniqueName == namesPrevious.get(previousInx).uniqueName) {
			return previousInx;
		}
	}

	// no luck, search the entire array
	for (previousInx = lowBound - 1; previousInx >= 0; previousInx--) {
		if (uniqueName == namesPrevious.get(previousInx).uniqueName) {
			return previousInx;
		}
	}
	for (previousInx = highBound; previousInx < itemCountPrevious; ++previousInx) {
		if (uniqueName == namesPrevious.get(previousInx).uniqueName) {
			return previousInx;
		}
	}

	return -1;
}

void InstanceManager::buildInstanceKeysZero() {
	instances.reserve(snapshotCurrent.getItemsCount());

	for (index currentInx = 0; currentInx < snapshotCurrent.getItemsCount(); ++currentInx) {
		const auto& item = namesCurrent.get(currentInx);

		InstanceInfo instanceKey;
		instanceKey.sortName = item.searchName;
		instanceKey.indices.current = currentInx;
		instanceKey.indices.previous = 0;

		if (blacklistManager.isAllowed(item.searchName, item.originalName)) {
			instances.push_back(instanceKey);
		} else if (keepDiscarded) {
			instancesDiscarded.push_back(instanceKey);
		}
	}
}

void InstanceManager::buildInstanceKeys() {
	instances.reserve(snapshotCurrent.getItemsCount());

	for (index current = 0; current < snapshotCurrent.getItemsCount(); ++current) {
		const pdh::ModifiedNameItem& item = namesCurrent.get(current);

		const auto previous = findPreviousName(item.uniqueName, current);
		if (previous < 0) {
			continue; // formatted values require previous item
		}

		InstanceInfo instanceKey;
		instanceKey.sortName = item.searchName;
		instanceKey.indices.current = current;
		instanceKey.indices.previous = previous;

		if (blacklistManager.isAllowed(item.searchName, item.originalName)) {
			instances.push_back(instanceKey);
		} else if (keepDiscarded) {
			instancesDiscarded.push_back(instanceKey);
		}
	}
}

void InstanceManager::sort(const ExpressionResolver& expressionResolver) {
	std::vector<InstanceInfo>& instances = rollup ? instancesRolledUp : this->instances;
	if (sortBy == SortBy::NONE || instances.empty()) {
		return;
	}

	if (sortBy == SortBy::INSTANCE_NAME) {
		switch (sortOrder) {
		case SortOrder::ASCENDING:
			std::sort(instances.begin(), instances.end(),
				[](const InstanceInfo& lhs, const InstanceInfo& rhs) {
				return lhs.sortName > rhs.sortName;
			});
			break;
		case SortOrder::DESCENDING:
			std::sort(instances.begin(), instances.end(),
				[](const InstanceInfo& lhs, const InstanceInfo& rhs) {
				return lhs.sortName < rhs.sortName;
			});
			break;
		default:
			log.error(L"unexpected sortOrder {}", sortOrder);
			break;
		}
		return;
	}

	switch (sortBy) {
	case SortBy::RAW_COUNTER:
	{
		if (rollup) {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.getRawRollup(sortRollupFunction, sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = calculateRaw(sortIndex, instance.indices);
			}
		}
		break;
	}
	case SortBy::FORMATTED_COUNTER:
	{
		if (!canGetFormatted()) {
			for (auto& instance : instances) {
				instance.sortValue = 0.0;
			}
			return;
		}
		if (rollup) {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.getFormattedRollup(sortRollupFunction, sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = calculateFormatted(sortIndex, instance.indices);
			}
		}
		break;
	}
	case SortBy::EXPRESSION:
	{
		if (rollup) {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.
					getExpressionRollup(sortRollupFunction, sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.getExpression(sortIndex, instance);
			}
		}
		break;
	}
	case SortBy::ROLLUP_EXPRESSION:
	{
		if (!rollup) {
			log.error(L"Resolving RollupExpression without rollup");
			return;
		}
		for (auto& instance : instances) {
			instance.sortValue = expressionResolver.getRollupExpression(sortIndex, instance);
		}
		break;
	}
	case SortBy::COUNT:
	{
		if (!rollup) {
			return;
		}
		for (auto& instance : instances) {
			instance.sortValue = static_cast<double>(instance.vectorIndices.size() + 1);
		}
		break;
	}
	default:
		log.error(L"unexpected sortBy {}", sortBy);
		return;
	}

	switch (sortOrder) {
	case SortOrder::ASCENDING:
		std::sort(instances.begin(), instances.end(), [](const InstanceInfo& lhs, const InstanceInfo& rhs) {
			return lhs.sortValue < rhs.sortValue;
		});
		break;
	case SortOrder::DESCENDING:
		std::sort(instances.begin(), instances.end(), [](const InstanceInfo& lhs, const InstanceInfo& rhs) {
			return lhs.sortValue > rhs.sortValue;
		});
		break;
	default:
		log.error(L"unexpected sortOrder {}", sortOrder);
		break;
	}
}

void InstanceManager::buildRollupKeys() {
	std::unordered_map<sview, std::optional<InstanceInfo>> mapRollupKeys(instances.size());

	for (const auto& instance : instances) {
		const Indices& indexes = instance.indices;

		auto &infoOpt = mapRollupKeys[instance.sortName];
		if (infoOpt.has_value()) {
			infoOpt.value().vectorIndices.push_back(indexes);
		} else {
			InstanceInfo item;
			item.sortName = instance.sortName;
			item.indices = indexes;

			infoOpt = item;
		}
	}

	instancesRolledUp.reserve(mapRollupKeys.size());
	for (auto& mapRollupKey : mapRollupKeys) {
		instancesRolledUp.emplace_back(mapRollupKey.second.value());
	}
}

const InstanceInfo* InstanceManager::findInstance(const Reference& ref, index sortedIndex) const {
	if (ref.named) {
		return findInstanceByName(ref, rollup);
	}

	const std::vector<InstanceInfo>& instances = rollup ? instancesRolledUp : this->instances;
	sortedIndex += indexOffset;
	if (sortedIndex < 0 || sortedIndex >= instances.size()) {
		return nullptr;
	}

	return &instances[sortedIndex];
}

const InstanceInfo* InstanceManager::findInstanceByName(const Reference& ref, bool useRollup) const {
	if (ref.discarded) {
		return findInstanceByNameInList(ref, instancesDiscarded, nameCacheDiscarded);
	}
	if (useRollup) {
		return findInstanceByNameInList(ref, instancesRolledUp, nameCacheRollup);
	} else {
		return findInstanceByNameInList(ref, instances, nameCache);
	}
}

const InstanceInfo* InstanceManager::findInstanceByNameInList(const Reference& ref, const std::vector<InstanceInfo> &instances,
	std::map<std::tuple<bool, bool, sview>, std::optional<const InstanceInfo*>>& cache) const {
	auto &itemOpt = cache[{ ref.useOrigName, ref.namePartialMatch, ref.name }];
	if (itemOpt.has_value()) {
		return itemOpt.value(); // already cached
	}

	MatchTestRecord testRecord { ref.name, ref.namePartialMatch };
	if (ref.useOrigName) {
		for (const auto& item : instances) {
			if (testRecord.match(namesCurrent.get(item.indices.current).originalName)) {
				itemOpt = &item;
				return itemOpt.value();
			}
		}
	} else {
		for (const auto& item : instances) {
			if (testRecord.match(item.sortName)) {
				itemOpt = &item;
				return itemOpt.value();
			}
		}
	}

	itemOpt = nullptr;
	return itemOpt.value();
}

long long InstanceManager::calculateRaw(index counterIndex, Indices originalIndexes) const {
	return snapshotCurrent.getItem(counterIndex, originalIndexes.current).FirstValue;
}

double InstanceManager::calculateFormatted(index counterIndex, Indices originalIndexes) const {
	return pdhWrapper.extractFormattedValue(
		counterIndex,
		snapshotCurrent.getItem(counterIndex, originalIndexes.current),
		snapshotPrevious.getItem(counterIndex, originalIndexes.previous)
	);
}

