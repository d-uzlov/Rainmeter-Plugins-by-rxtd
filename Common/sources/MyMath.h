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
	//
	// Utility class with small math snippets
	//
	class MyMath {
	public:
		static const double pi;

		// bit twiddling.
		// returns approximately a**b
		[[nodiscard]]
		static double fastPow(double a, double b);

		// bit twiddling
		[[nodiscard]]
		static float fastLog2(float val);

		// bit twiddling
		[[nodiscard]]
		static float fastSqrt(float value);

		// A unified way to convert decibels to absolute values,
		[[nodiscard]]
		static float db2amplitude(float value);

		// A unified way to convert decibels to absolute values,
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
