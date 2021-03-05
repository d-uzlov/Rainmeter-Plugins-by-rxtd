// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "SortOrder.h"
#include "rxtd/perfmon/Reference.h"
#include "rxtd/perfmon/pdh/NamesManager.h"

namespace rxtd::perfmon {
	class InstanceManagerUtilities {
	public:
		template<typename InstanceInfo>
		static void sortByName(array_span<InstanceInfo> instances, SortOrder sortOrder) {
			switch (sortOrder) {
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
			}
		}

		template<typename Solver, typename InstanceInfo>
		static void sort(array_span<InstanceInfo> instances, const Solver& expressionResolver, const Reference& ref, SortOrder sortOrder) {
			// Reference ref;
			// ref.counter = options.sortIndex;
			// ref.rollupFunction = options.sortRollupFunction;

			for (auto& instance : instances) {
				instance.sortValue = expressionResolver.resolveReference(ref, instance.indices);
			}

			switch (sortOrder) {
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
			}
		}

		template<typename InstanceType, typename CacheType>
		static const InstanceType* findInstanceByNameInList(array_view<InstanceType> instances, const Reference& ref, CacheType& cache, const pdh::NamesManager& namesManager) {
			auto& itemOpt = cache[{ ref.namePattern, ref.useOrigName }];
			if (itemOpt.has_value()) {
				return itemOpt.value(); // already cached
			}

			if (ref.useOrigName) {
				for (const auto& item : instances) {
					if (ref.namePattern.match(namesManager.get(item.getFirst().current).originalName)) {
						itemOpt = &item;
						return itemOpt.value();
					}
				}
			} else {
				for (const auto& item : instances) {
					if (ref.namePattern.match(item.sortName)) {
						itemOpt = &item;
						return itemOpt.value();
					}
				}
			}

			itemOpt = nullptr;
			return itemOpt.value();
		}
	};
}
