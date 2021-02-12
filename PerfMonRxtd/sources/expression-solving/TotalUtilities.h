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
#include "InstanceManager.h"

namespace rxtd::perfmon {
	class TotalUtilities {
	public:
		template<typename CacheKey, typename CacheValue, typename Callable>
		static double getTotal(CacheHelper<CacheKey, CacheValue>& cache, array_view<InstanceInfo> instances, RollupFunction rollupFunction, Callable callable) {
			auto computeFunction = [&]() {
				double value = 0.0;

				switch (rollupFunction) {
				case RollupFunction::eSUM: {
					for (auto item : instances) {
						value += callable(item.indices);
					}
					break;
				}
				case RollupFunction::eAVERAGE: {
					if (instances.empty()) {
						break;
					}
					for (auto item : instances) {
						value += callable(item.indices);
					}
					value /= double(instances.size());
					break;
				}
				case RollupFunction::eMINIMUM: {
					value = std::numeric_limits<double>::max();
					for (auto item : instances) {
						value = std::min(value, callable(item.indices));
					}
					break;
				}
				case RollupFunction::eMAXIMUM: {
					value = std::numeric_limits<double>::min();
					for (auto item : instances) {
						value = std::max(value, callable(item.indices));
					}
					break;
				}
				case RollupFunction::eFIRST:
					if (!instances.empty()) {
						value = callable(instances[0].indices);
					}
					break;
				}

				return value;
			};

			return cache.getOrCompute({ counterIndex, rollupFunction }, computeFunction);
		}
	};
}
