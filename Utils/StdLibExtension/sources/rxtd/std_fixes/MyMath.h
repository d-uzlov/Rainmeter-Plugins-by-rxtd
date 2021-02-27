/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <cmath>

namespace rxtd::std_fixes {
	//
	// Utility class with small math snippets
	//
	class MyMath {
	public:
		static const double pi;
		static const float pif;
		
		// A unified way to convert decibels to absolute values,
		template<typename Float>
		[[nodiscard]]
		static Float db2amplitude(Float value) {
			return std::pow(Float(10.0), value / Float(10.0));
		}

		template<typename TOut, typename TIn>
		[[nodiscard]]
		static TOut roundTo(TIn value) {
			static_assert(std::is_integral<TOut>::value);

			if constexpr (sizeof(TOut) == sizeof(long)) {
				return static_cast<TOut>(std::lround(value));
			} else if constexpr (sizeof(TOut) == sizeof(long long)) {
				return static_cast<TOut>(std::llround(value));
			} else {
				// ReSharper disable once CppStaticAssertFailure
				static_assert(false);
				return {};
			}
		}

		template<typename TOut, typename TIn>
		[[nodiscard]]
		static TOut roundToVar(TIn value, TOut dummy = {}) {
			return roundTo<TOut>(value);
		}
	};
}
