/* 
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::perfmon {
	template<typename Key, typename Value>
	class CacheHelper {
		using KeyType = Key;
		using ValueType = Value;
		
		std::map<KeyType, std::optional<ValueType>> values;

	public:
		void reset() {
			values.clear();
		}

		template<typename Callable>
		ValueType getOrCompute(const KeyType& key, Callable callable) {
			auto& valueOpt = values[key];
			
			if (!valueOpt.has_value()) {
				valueOpt = callable();
			}
			
			return valueOpt.value();
		}
	};
}
