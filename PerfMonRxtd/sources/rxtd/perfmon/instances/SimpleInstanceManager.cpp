// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#include "SimpleInstanceManager.h"

#include "InstanceManagerUtilities.h"
#include "rxtd/perfmon/expressions/RollupExpressionResolver.h"
#include "rxtd/perfmon/expressions/SimpleExpressionSolver.h"

using rxtd::perfmon::SimpleInstanceManager;

SimpleInstanceManager::SimpleInstanceManager(Logger log, const pdh::PdhWrapper& phWrapper) :
	log(std::move(log)), pdhWrapper(phWrapper) { }

void SimpleInstanceManager::setNameModificationType(pdh::NamesManager::ModificationType value) {
	namesManager.setModificationType(value);
}

void SimpleInstanceManager::update() {
	instances.clear();
	instancesDiscarded.clear();

	nameCaches.reset();

	if (snapshotCurrent.isEmpty()) {
		return;
	}

	std::swap(idsCurrent, idsPrevious);
	idsCurrent.resize(static_cast<size_t>(snapshotCurrent.getItemsCount()));
	namesManager.createModifiedNames(snapshotCurrent, processIdsSnapshot, idsCurrent);

	if (snapshotPrevious.isEmpty()) {
		buildInstanceKeysZero();
	} else {
		buildInstanceKeys();
	}
}

void SimpleInstanceManager::sort(const expressions::SimpleExpressionSolver& simpleExpressionSolver) {
	switch (options.sortInfo.sortBy) {
	case SortBy::eNONE: return;
	case SortBy::eINSTANCE_NAME:
		InstanceManagerUtilities::sortByName(array_span<InstanceInfo>{ instances }, options.sortInfo.sortOrder);
		break;
	case SortBy::eVALUE: {
		Reference ref;

		ref.type = options.sortInfo.sortByValueInformation.expressionType;
		ref.counter = options.sortInfo.sortByValueInformation.sortIndex;
		ref.rollupFunction = options.sortInfo.sortByValueInformation.sortRollupFunction;

		InstanceManagerUtilities::sort(array_span<InstanceInfo>{ instances }, simpleExpressionSolver, ref, options.sortInfo.sortOrder);
		break;
	}
	}
}

rxtd::index SimpleInstanceManager::findPreviousName(pdh::UniqueInstanceId uniqueId, index hint) const {
	// try to find a match for the current instance name in the previous buffer
	// counter buffers tend to be *mostly* aligned, so we'll try to short-circuit a full search

	array_view<pdh::UniqueInstanceId> idsPreviousView = idsPrevious;
	
	const auto itemCountPrevious = idsPreviousView.size();

	// try for a direct hit
	auto previousInx = std::clamp<index>(hint, 0, itemCountPrevious - 1);

	if (uniqueId == idsPreviousView[previousInx]) {
		return previousInx;
	}

	// try a window around currentIndex
	constexpr index windowSize = 5;

	const auto lowBound = std::clamp<index>(hint - windowSize, 0, itemCountPrevious - 1);
	const auto highBound = std::clamp<index>(hint + windowSize, 0, itemCountPrevious - 1);

	for (previousInx = lowBound; previousInx <= highBound; ++previousInx) {
		if (uniqueId == idsPreviousView[previousInx]) {
			return previousInx;
		}
	}

	// no luck, search the entire array
	for (previousInx = lowBound - 1; previousInx >= 0; previousInx--) {
		if (uniqueId == idsPreviousView[previousInx]) {
			return previousInx;
		}
	}
	for (previousInx = highBound; previousInx < itemCountPrevious; ++previousInx) {
		if (uniqueId == idsPreviousView[previousInx]) {
			return previousInx;
		}
	}

	return -1;
}

void SimpleInstanceManager::buildInstanceKeysZero() {
	instances.reserve(static_cast<size_t>(snapshotCurrent.getItemsCount()));

	for (index currentIndex = 0; currentIndex < snapshotCurrent.getItemsCount(); ++currentIndex) {
		const auto& item = namesManager.get(currentIndex);

		InstanceInfo instanceKey;
		instanceKey.sortName = item.searchName;
		instanceKey.indices.current = static_cast<int_fast16_t>(currentIndex);
		instanceKey.indices.previous = 0;

		if (blacklistManager.isAllowed(item.searchName, item.originalName)) {
			instances.push_back(instanceKey);
		} else if (options.keepDiscarded) {
			instancesDiscarded.push_back(instanceKey);
		}
	}
}

void SimpleInstanceManager::buildInstanceKeys() {
	instances.reserve(static_cast<size_t>(snapshotCurrent.getItemsCount()));

	for (index current = 0; current < snapshotCurrent.getItemsCount(); ++current) {
		const auto item = namesManager.get(current);

		const auto previous = findPreviousName(idsCurrent[static_cast<size_t>(current)], current);
		if (previous < 0) {
			continue; // formatted values require previous item
		}

		InstanceInfo instanceKey;
		instanceKey.sortName = item.searchName;
		instanceKey.indices.current = static_cast<int_fast16_t>(current);
		instanceKey.indices.previous = static_cast<int_fast16_t>(previous);

		if (blacklistManager.isAllowed(item.searchName, item.originalName)) {
			instances.push_back(instanceKey);
		} else if (options.keepDiscarded) {
			instancesDiscarded.push_back(instanceKey);
		}
	}
}

const SimpleInstanceManager::InstanceInfo* SimpleInstanceManager::findSimpleInstance(const Reference& ref, index sortedIndex) const {
	if (!ref.namePattern.isEmpty()) {
		return findSimpleInstanceByName(ref);
	}

	sortedIndex += indexOffset;
	if (sortedIndex < 0 || sortedIndex >= static_cast<index>(instances.size())) {
		return nullptr;
	}

	return &instances[static_cast<size_t>(sortedIndex)];
}

const SimpleInstanceManager::InstanceInfo* SimpleInstanceManager::findSimpleInstanceByName(const Reference& ref) const {
	if (ref.discarded) {
		return InstanceManagerUtilities::findInstanceByNameInList(array_view<InstanceInfo>{ instancesDiscarded }, ref, nameCaches.discarded, namesManager);
	}
	return InstanceManagerUtilities::findInstanceByNameInList(array_view<InstanceInfo>{ instances }, ref, nameCaches.simple, namesManager);
}

double SimpleInstanceManager::calculateRaw(index counterIndex, Indices originalIndexes) const {
	return static_cast<double>(snapshotCurrent.getItem(counterIndex, originalIndexes.current).FirstValue);
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
