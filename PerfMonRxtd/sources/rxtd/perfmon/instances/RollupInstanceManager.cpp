// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#include "RollupInstanceManager.h"

#include "InstanceManagerUtilities.h"
#include "rxtd/perfmon/expressions/RollupExpressionResolver.h"
#include "rxtd/perfmon/expressions/SimpleExpressionSolver.h"

using rxtd::perfmon::RollupInstanceManager;

void RollupInstanceManager::update() {
	instancesRolledUp.clear();

	nameCaches.reset();

	buildRollupKeys();
}

void RollupInstanceManager::sort(const expressions::RollupExpressionResolver& simpleExpressionSolver) {
	switch (options.sortInfo.sortBy) {
	case SortBy::eNONE: return;
	case SortBy::eINSTANCE_NAME:
		InstanceManagerUtilities::sortByName(array_span<RollupInstanceInfo>{ instancesRolledUp }, options.sortInfo.sortOrder);
		break;
	case SortBy::eVALUE: {
		Reference ref;

		ref.type = options.sortInfo.sortByValueInformation.expressionType;
		ref.counter = options.sortInfo.sortByValueInformation.sortIndex;
		ref.rollupFunction = options.sortInfo.sortByValueInformation.sortRollupFunction;

		InstanceManagerUtilities::sort(array_span<RollupInstanceInfo>{ instancesRolledUp }, simpleExpressionSolver, ref, options.sortInfo.sortOrder);
		break;
	}
	}
}

void RollupInstanceManager::buildRollupKeys() {
	std::unordered_map<sview, RollupInstanceInfo> mapRollupKeys;
	mapRollupKeys.reserve(static_cast<size_t>(simpleInstanceManager.getInstances().size()));

	for (const auto& instance : simpleInstanceManager.getInstances()) {
		auto& item = mapRollupKeys[instance.sortName];
		item.indices.push_back(instance.indices);
	}

	instancesRolledUp.reserve(mapRollupKeys.size());
	for (auto& [key, value] : mapRollupKeys) {
		instancesRolledUp.emplace_back(std::move(value));
	}
}

const RollupInstanceManager::RollupInstanceInfo* RollupInstanceManager::findRollupInstance(const Reference& ref, index sortedIndex) const {
	if (!ref.namePattern.isEmpty()) {
		return findRollupInstanceByName(ref);
	}

	sortedIndex += indexOffset;
	if (sortedIndex < 0 || sortedIndex >= static_cast<index>(simpleInstanceManager.getInstances().size())) {
		return nullptr;
	}

	return &instancesRolledUp[static_cast<size_t>(sortedIndex)];
}

const RollupInstanceManager::RollupInstanceInfo* RollupInstanceManager::findRollupInstanceByName(const Reference& ref) const {
	return InstanceManagerUtilities::findInstanceByNameInList(
		array_view<RollupInstanceInfo>{ instancesRolledUp },
		ref,
		nameCaches.rollup,
		simpleInstanceManager.getNamesManager()
	);
}
