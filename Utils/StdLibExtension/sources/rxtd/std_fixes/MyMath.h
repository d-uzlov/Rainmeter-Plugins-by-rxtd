// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

#include <cmath>

namespace rxtd::std_fixes {
	//
	// Utility class with small math snippets
	//
	class MyMath {
	public:
		template<typename Float>
		static Float pi() {
			return std::acos(static_cast<Float>(-1.0));
		}

		/// <summary>
		/// Compares 2 floating point values for rough equality.
		/// Source: https://stackoverflow.com/a/32334103
		/// </summary>
		/// <param name="a">First number to compare</param>
		/// <param name="b">Second number to compare</param>
		/// <param name="epsilonMultiplier">Relative error is epsilonMultiplier*epsilon<Float></param>
		/// <param name="absoluteThresholdMultiplier">Absolute error is absoluteThresholdMultiplier*epsilon<Float></param>
		/// <returns>True if values are close based on epsilon and absoluteThreshold, false otherwise.</returns>
		template<typename Float>
		static bool checkFloatEqual(
			Float a, Float b,
			Float epsilonMultiplier = static_cast<Float>(64.0),
			Float absoluteThresholdMultiplier = static_cast<Float>(1.0)
		) {
			static_assert(
				std::is_same<Float, float>::value || std::is_same<Float, double>::value || std::is_same<Float, long double>::value,
				"Only native float types are supported"
			);
			assert(epsilonMultiplier >= static_cast<Float>(0.0));
			assert(absoluteThresholdMultiplier >= static_cast<Float>(0.0));
			constexpr Float floatEpsilon = static_cast<Float>(std::numeric_limits<float>::epsilon());
			const auto epsilon = epsilonMultiplier * floatEpsilon;
			const auto absoluteThreshold = absoluteThresholdMultiplier * floatEpsilon;

			if (a == b) return true;

			auto diff = std::abs(a - b);
			auto norm = std::min(std::abs(a + b), std::numeric_limits<Float>::max());
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
