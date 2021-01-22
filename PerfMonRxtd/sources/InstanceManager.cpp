/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "InstanceManager.h"
#include "ExpressionResolver.h"

using namespace perfmon;

InstanceManager::InstanceManager(utils::Rainmeter::Logger& log, const pdh::PdhWrapper& phWrapper) :
	log(log), pdhWrapper(phWrapper) { }

void InstanceManager::checkIndices(index counters, index expressions, index rollupExpressions) {
	index checkCount = 0;

	switch (options.sortBy) {
	case SortBy::eNONE: return;
	case SortBy::eINSTANCE_NAME: return;
	case SortBy::eRAW_COUNTER: [[fallthrough]];
	case SortBy::eFORMATTED_COUNTER:
		checkCount = counters;
		break;
	case SortBy::eEXPRESSION: {
		if (expressions <= 0) {
			log.error(L"Sort by Expression requires at least 1 Expression specified. Set to None.");
			options.sortBy = SortBy::eNONE;
			return;
		}
		checkCount = expressions;
		break;
	}
	case SortBy::eROLLUP_EXPRESSION: {
		if (!rollupExpressions) {
			log.error(L"RollupExpressions can't be used for sort if rollup is disabled. Set to None.");
			options.sortBy = SortBy::eNONE;
			return;
		}
		if (rollupExpressions <= 0) {
			log.error(L"Sort by RollupExpression requires at least 1 RollupExpression specified. Set to None.");
			options.sortBy = SortBy::eNONE;
			return;
		}
		checkCount = rollupExpressions;
		break;
	}
	case SortBy::eCOUNT: return;
	}

	if (options.sortIndex >= 0 && options.sortIndex < checkCount) {
		return;
	}
	log.error(L"SortIndex {} is out of bounds (must be in [0; {}]). Set to 0.", expressions, options.sortIndex);
	options.sortIndex = 0;
	return;
}

void InstanceManager::setNameModificationType(pdh::NamesManager::ModificationType value) {
	namesManager.setModificationType(value);
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

	std::swap(idsCurrent, idsPrevious);
	idsCurrent.resize(snapshotCurrent.getItemsCount());
	namesManager.createModifiedNames(snapshotCurrent, processIdsSnapshot, idsCurrent);

	if (snapshotPrevious.isEmpty()) {
		buildInstanceKeysZero();
	} else {
		buildInstanceKeys();
	}
	if (options.rollup) {
		buildRollupKeys();
	}
}

index InstanceManager::findPreviousName(pdh::UniqueInstanceId uniqueId, index hint) const {
	// try to find a match for the current instance name in the previous names buffer
	// use the unique name for this search because we need a unique match
	// counter buffers tend to be *mostly* aligned, so we'll try to short-circuit a full search

	const auto itemCountPrevious = idsPrevious.size();

	// try for a direct hit
	auto previousInx = std::clamp<index>(hint, 0, itemCountPrevious - 1);

	if (uniqueId == idsPrevious[previousInx]) {
		return previousInx;
	}

	// try a window around currentIndex
	constexpr index windowSize = 5;

	const auto lowBound = std::clamp<index>(hint - windowSize, 0, itemCountPrevious - 1);
	const auto highBound = std::clamp<index>(hint + windowSize, 0, itemCountPrevious - 1);

	for (previousInx = lowBound; previousInx <= highBound; ++previousInx) {
		if (uniqueId == idsPrevious[previousInx]) {
			return previousInx;
		}
	}

	// no luck, search the entire array
	for (previousInx = lowBound - 1; previousInx >= 0; previousInx--) {
		if (uniqueId == idsPrevious[previousInx]) {
			return previousInx;
		}
	}
	for (previousInx = highBound; previousInx < itemCountPrevious; ++previousInx) {
		if (uniqueId == idsPrevious[previousInx]) {
			return previousInx;
		}
	}

	return -1;
}

void InstanceManager::buildInstanceKeysZero() {
	instances.reserve(snapshotCurrent.getItemsCount());

	for (index currentIndex = 0; currentIndex < snapshotCurrent.getItemsCount(); ++currentIndex) {
		const auto& item = namesManager.get(currentIndex);

		InstanceInfo instanceKey;
		instanceKey.sortName = item.searchName;
		instanceKey.indices.current = int_fast16_t(currentIndex);
		instanceKey.indices.previous = 0;

		if (blacklistManager.isAllowed(item.searchName, item.originalName)) {
			instances.push_back(instanceKey);
		} else if (options.keepDiscarded) {
			instancesDiscarded.push_back(instanceKey);
		}
	}
}

void InstanceManager::buildInstanceKeys() {
	instances.reserve(snapshotCurrent.getItemsCount());

	for (index current = 0; current < snapshotCurrent.getItemsCount(); ++current) {
		const auto item = namesManager.get(current);

		const auto previous = findPreviousName(idsCurrent[current], current);
		if (previous < 0) {
			continue; // formatted values require previous item
		}

		InstanceInfo instanceKey;
		instanceKey.sortName = item.searchName;
		instanceKey.indices.current = int_fast16_t(current);
		instanceKey.indices.previous = int_fast16_t(previous);

		if (blacklistManager.isAllowed(item.searchName, item.originalName)) {
			instances.push_back(instanceKey);
		} else if (options.keepDiscarded) {
			instancesDiscarded.push_back(instanceKey);
		}
	}
}

void InstanceManager::sort(const ExpressionResolver& expressionResolver) {
	std::vector<InstanceInfo>& instances = options.rollup ? instancesRolledUp : this->instances;
	if (options.sortBy == SortBy::eNONE || instances.empty()) {
		return;
	}

	if (options.sortBy == SortBy::eINSTANCE_NAME) {
		switch (options.sortOrder) {
		case SortOrder::eASCENDING:
			std::sort(
				instances.begin(), instances.end(),
				[](const InstanceInfo& lhs, const InstanceInfo& rhs) {
					return lhs.sortName > rhs.sortName;
				}
			);
			break;
		case SortOrder::eDESCENDING:
			std::sort(
				instances.begin(), instances.end(),
				[](const InstanceInfo& lhs, const InstanceInfo& rhs) {
					return lhs.sortName < rhs.sortName;
				}
			);
			break;
		default:
			log.error(L"unexpected sortOrder {}", options.sortOrder);
			break;
		}
		return;
	}

	switch (options.sortBy) {
	case SortBy::eRAW_COUNTER: {
		if (options.rollup) {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.getRawRollup(options.sortRollupFunction, options.sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = calculateRaw(options.sortIndex, instance.indices);
			}
		}
		break;
	}
	case SortBy::eFORMATTED_COUNTER: {
		if (!canGetFormatted()) {
			for (auto& instance : instances) {
				instance.sortValue = 0.0;
			}
			return;
		}
		if (options.rollup) {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.getFormattedRollup(options.sortRollupFunction, options.sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = calculateFormatted(options.sortIndex, instance.indices);
			}
		}
		break;
	}
	case SortBy::eEXPRESSION: {
		if (options.rollup) {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.
					getExpressionRollup(options.sortRollupFunction, options.sortIndex, instance);
			}
		} else {
			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.getExpression(options.sortIndex, instance);
			}
		}
		break;
	}
	case SortBy::eROLLUP_EXPRESSION: {
		if (!options.rollup) {
			log.error(L"Resolving RollupExpression without rollup");
			return;
		}
		for (auto& instance : instances) {
			instance.sortValue = expressionResolver.getRollupExpression(options.sortIndex, instance);
		}
		break;
	}
	case SortBy::eCOUNT: {
		if (!options.rollup) {
			return;
		}
		for (auto& instance : instances) {
			instance.sortValue = static_cast<double>(instance.vectorIndices.size() + 1);
		}
		break;
	}
	default:
		log.error(L"unexpected sortBy {}", options.sortBy);
		return;
	}

	switch (options.sortOrder) {
	case SortOrder::eASCENDING:
		std::sort(
			instances.begin(), instances.end(), [](const InstanceInfo& lhs, const InstanceInfo& rhs) {
				return lhs.sortValue < rhs.sortValue;
			}
		);
		break;
	case SortOrder::eDESCENDING:
		std::sort(
			instances.begin(), instances.end(), [](const InstanceInfo& lhs, const InstanceInfo& rhs) {
				return lhs.sortValue > rhs.sortValue;
			}
		);
		break;
	default:
		log.error(L"unexpected sortOrder {}", options.sortOrder);
		break;
	}
}

void InstanceManager::buildRollupKeys() {
	std::unordered_map<sview, InstanceInfo> mapRollupKeys;
	mapRollupKeys.reserve(instances.size());

	for (const auto& instance : instances) {
		auto& item = mapRollupKeys[instance.sortName];
		// in this function I use .sortValue to count indices in InstanceInfo
		if (item.sortValue == 0.0) {
			item.indices = instance.indices;
		} else {
			item.vectorIndices.push_back(instance.indices);
		}
		item.sortValue += 1.0;
	}

	instancesRolledUp.reserve(mapRollupKeys.size());
	for (auto& [key, value] : mapRollupKeys) {
		if (value.sortValue != 0.0) {
			instancesRolledUp.emplace_back(std::move(value));
		}
	}
}

const InstanceInfo* InstanceManager::findInstance(const Reference& ref, index sortedIndex) const {
	if (ref.named) {
		return findInstanceByName(ref, options.rollup);
	}

	const std::vector<InstanceInfo>& instances = options.rollup ? instancesRolledUp : this->instances;
	sortedIndex += indexOffset;
	if (sortedIndex < 0 || sortedIndex >= index(instances.size())) {
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

const InstanceInfo* InstanceManager::findInstanceByNameInList(const Reference& ref, array_view<InstanceInfo> instances, CacheType& cache) const {
	auto& itemOpt = cache[{ ref.useOrigName, ref.namePartialMatch, ref.name }];
	if (itemOpt.has_value()) {
		return itemOpt.value(); // already cached
	}

	MatchTestRecord testRecord{ ref.name, ref.namePartialMatch };
	if (ref.useOrigName) {
		for (const auto& item : instances) {
			if (testRecord.match(namesManager.get(item.indices.current).originalName)) {
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

double InstanceManager::calculateRaw(index counterIndex, Indices originalIndexes) const {
	return double(snapshotCurrent.getItem(counterIndex, originalIndexes.current).FirstValue);
}

double InstanceManager::calculateFormatted(index counterIndex, Indices originalIndexes) const {
	return pdhWrapper.extractFormattedValue(
		counterIndex,
		snapshotCurrent.getItem(counterIndex, originalIndexes.current),
		snapshotPrevious.getItem(counterIndex, originalIndexes.previous)
	);
}
