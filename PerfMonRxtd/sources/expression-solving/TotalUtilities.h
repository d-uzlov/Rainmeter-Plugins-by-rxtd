/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "CacheHelper.h"
#include "enums.h"

namespace rxtd::perfmon {
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
				value /= double(instances.size());
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
