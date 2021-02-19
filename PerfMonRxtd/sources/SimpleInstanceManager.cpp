/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SimpleInstanceManager.h"
#include "expressions/SimpleExpressionSolver.h"
#include "expressions/RollupExpressionResolver.h"

using namespace rxtd::perfmon;

SimpleInstanceManager::SimpleInstanceManager(Logger log, const pdh::PdhWrapper& phWrapper) :
	log(std::move(log)), pdhWrapper(phWrapper) { }

void SimpleInstanceManager::checkIndices(index counters, index expressions, index rollupExpressions) {
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

void SimpleInstanceManager::setNameModificationType(pdh::NamesManager::ModificationType value) {
	namesManager.setModificationType(value);
}

void SimpleInstanceManager::update() {
	instances.clear();
	instancesRolledUp.clear();
	instancesDiscarded.clear();

	nameCaches.reset();

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

rxtd::index SimpleInstanceManager::findPreviousName(pdh::UniqueInstanceId uniqueId, index hint) const {
	// try to find a match for the current instance name in the previous buffer
	// counter buffers tend to be *mostly* aligned, so we'll try to short-circuit a full search

	const auto itemCountPrevious = index(idsPrevious.size());

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

void SimpleInstanceManager::buildInstanceKeysZero() {
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

void SimpleInstanceManager::buildInstanceKeys() {
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

void SimpleInstanceManager::buildRollupKeys() {
	std::unordered_map<sview, RollupInstanceInfo> mapRollupKeys;
	mapRollupKeys.reserve(instances.size());

	for (const auto& instance : instances) {
		auto& item = mapRollupKeys[instance.sortName];
		item.indices.push_back(instance.indices);
	}

	instancesRolledUp.reserve(mapRollupKeys.size());
	for (auto& [key, value] : mapRollupKeys) {
		instancesRolledUp.emplace_back(std::move(value));
	}
}

const InstanceInfo* SimpleInstanceManager::findSimpleInstance(const Reference& ref, index sortedIndex) const {
	if (!ref.namePattern.isEmpty()) {
		return findSimpleInstanceByName(ref);
	}

	sortedIndex += indexOffset;
	if (sortedIndex < 0 || sortedIndex >= index(instances.size())) {
		return nullptr;
	}

	return &instances[sortedIndex];
}

const RollupInstanceInfo* SimpleInstanceManager::findRollupInstance(const Reference& ref, index sortedIndex) const {
	if (!ref.namePattern.isEmpty()) {
		return findRollupInstanceByName(ref);
	}

	sortedIndex += indexOffset;
	if (sortedIndex < 0 || sortedIndex >= index(instances.size())) {
		return nullptr;
	}

	return &instancesRolledUp[sortedIndex];
}

const InstanceInfo* SimpleInstanceManager::findSimpleInstanceByName(const Reference& ref) const {
	if (ref.discarded) {
		return findInstanceByNameInList(ref, array_view<InstanceInfo>{ instancesDiscarded }, nameCaches.discarded);
	}
	return findInstanceByNameInList(ref, array_view<InstanceInfo>{ instances }, nameCaches.simple);
}

const RollupInstanceInfo* SimpleInstanceManager::findRollupInstanceByName(const Reference& ref) const {
	return findInstanceByNameInList(ref, array_view<RollupInstanceInfo>{ instancesRolledUp }, nameCaches.rollup);
}

double SimpleInstanceManager::calculateRaw(index counterIndex, Indices originalIndexes) const {
	return double(snapshotCurrent.getItem(counterIndex, originalIndexes.current).FirstValue);
}

double SimpleInstanceManager::calculateFormatted(index counterIndex, Indices originalIndexes) const {
	if (!canGetFormatted()) {
		return 0.0;
	}

	return pdhWrapper.extractFormattedValue(
		counterIndex,
		snapshotCurrent.getItem(counterIndex, originalIndexes.current),
		snapshotPrevious.getItem(counterIndex, originalIndexes.previous)
	);
}
