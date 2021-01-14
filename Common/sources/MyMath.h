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

namespace rxtd::utils {
	class MyMath {
	public:
		static const double pi;

		// return approximately a**b
		[[nodiscard]]
		static double fastPow(double a, double b);

		[[nodiscard]]
		static float fastLog2(float val);

		[[nodiscard]]
		static float fastSqrt(float value);

		[[nodiscard]]
		static float db2amplitude(float value);

		[[nodiscard]]
		static double db2amplitude(double value);

		template<typename TOut, typename TIn>
		[[nodiscard]]
		static TOut roundTo(TIn value) {
			static_assert(std::is_integral<TOut>::value);

			if constexpr (sizeof(TOut) == sizeof(long)) {
				return static_cast<TOut>(std::lround(value));
			} else if constexpr (sizeof(TOut) == sizeof(long long)) {
				return static_cast<TOut>(std::llround(value));
			} else {
				static_assert(false);
				return {};
			}
		}

		template<typename TOut, typename TIn>
		[[nodiscard]]
		static TOut roundToVar(TIn value, TOut) {
			static_assert(std::is_integral<TOut>::value);
			static_assert(sizeof(TOut) == sizeof(long) || sizeof(TOut) == sizeof(long long));

			if constexpr (sizeof(TOut) == sizeof(long)) {
				return static_cast<TOut>(std::lround(value));
			} else if constexpr (sizeof(TOut) == sizeof(long long)) {
				return static_cast<TOut>(std::llround(value));
			} else {
				return {};
			}
		}

	};
}
