// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

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
