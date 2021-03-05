// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once
#include "CacheHelper.h"
#include "rxtd/perfmon/enums.h"

namespace rxtd::perfmon::expressions {
	class TotalUtilities {
	public:
		template<typename CacheKey, typename InstanceStruct, typename Callable>
		static double getTotal(CacheHelper<CacheKey, double>& cache, array_view<InstanceStruct> instances, index counterIndex, RollupFunction rollupFunction, Callable callable) {
			return cache.getOrCompute(
				{ counterIndex, rollupFunction },
				[=]() {
					return calculateTotal(instances, rollupFunction, callable);
				}
			);
		}

		template<typename InstanceStruct, typename Callable>
		static double calculateTotal(array_view<InstanceStruct> instances, RollupFunction rollupFunction, Callable callable) {
			double value = 0.0;

			switch (rollupFunction) {
			case RollupFunction::eSUM: {
				for (const auto& item : instances) {
					value += callable(item);
				}
				break;
			}
			case RollupFunction::eAVERAGE: {
				if (instances.empty()) {
					break;
				}
				for (const auto& item : instances) {
					value += callable(item);
				}
				value /= static_cast<double>(instances.size());
				break;
			}
			case RollupFunction::eMINIMUM: {
				value = std::numeric_limits<double>::max();
				for (const auto& item : instances) {
					value = std::min(value, callable(item));
				}
				break;
			}
			case RollupFunction::eMAXIMUM: {
				value = std::numeric_limits<double>::min();
				for (const auto& item : instances) {
					value = std::max(value, callable(item));
				}
				break;
			}
			case RollupFunction::eFIRST:
				if (!instances.empty()) {
					value = callable(instances[0]);
				}
				break;
			}

			return value;
		}
	};
}
