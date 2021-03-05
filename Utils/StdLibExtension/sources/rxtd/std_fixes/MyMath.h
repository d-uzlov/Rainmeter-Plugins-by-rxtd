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

		/// <summary>
		/// Compares 2 floating point values for rough equality.
		/// Source: https://stackoverflow.com/a/32334103
		/// </summary>
		/// <typeparam name="Float"></typeparam>
		/// <param name="a">First number to compare</param>
		/// <param name="b">Second number to compare</param>
		/// <param name="epsilon">Max relative error</param>
		/// <param name="absoluteThreshold">Max absolute error</param>
		/// <returns>True if values are close based on epsilon and absoluteThreshold, false otherwise.</returns>
		template<typename Float>
		static bool checkFloatEqual(
			Float a, Float b,
			Float epsilon = static_cast<Float>(128) * std::numeric_limits<Float>::epsilon(),
			Float absoluteThreshold = std::numeric_limits<Float>::min()
		) {
			assert(std::numeric_limits<Float>::epsilon() <= epsilon);
			assert(epsilon < 1.f);

			if (a == b) return true;

			auto diff = std::abs(a - b);
			auto norm = std::min(std::abs(a) + std::abs(b), std::numeric_limits<Float>::max());
			// or even faster: std::min(std::abs(a + b), std::numeric_limits<float>::max());
			// keeping this commented out until I update figures below
			return diff < std::max(absoluteThreshold, epsilon * norm);
		}

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
