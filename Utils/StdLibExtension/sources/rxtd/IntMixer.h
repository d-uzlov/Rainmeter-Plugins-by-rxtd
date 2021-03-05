// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd {
	//
	// This class allows mass interpolation of integers without floating pointer arithmetic.
	// For cases when integer to/from double conversion is too slow.
	//
	// Note:
	//	Watch ouy for int overflow.
	//	All input values are effectively shifted by precision value,
	//	so if (value << precision) may be outside of IntType range
	//	then consider either increasing IntType size or decreasing precision.
	//
	// How it works:
	//
	// Let's mix A and B with a factor of Q
	//	Result = A * Q + B * (1 - Q)
	//	Result = (A * Q * W + B * (1 - Q) * W) / W, where W can be anything except for zero and infinity
	//	Result = (A * Q * W + B * (W - Q * W)) / W
	// Let W = 2**U. Then:
	//	Result = (A * Q * 2**U + B * (2**U - Q * 2**U)) / 2**U
	// Conveniently X / 2**U == X >> U
	//
	// Let precision = U
	// Let shifted = 2**U
	// Let integerFactor = Q * 2**U
	//
	template<typename IntType = int_fast32_t, uint8_t precision = 8>
	class IntMixer {
		static_assert(std::is_integral<IntType>::value, "Only integral types can be used for integer interpolation");
		static_assert(precision <= (sizeof(IntType) * 8 - 2));

		using MixType = IntType;
		MixType integerFactor = 0;
		static constexpr MixType shifted = 1 << precision;

	public:
		IntMixer() = default;

		IntMixer(double factor) {
			setFactor(factor);
		}

		IntMixer(float factor) {
			setFactor(factor);
		}

		void setFactor(double factor) {
			integerFactor = MixType(factor * shifted);
		}

		void setFactor(float factor) {
			integerFactor = MixType(factor * shifted);
		}

		// range is from 0 to (1 << precision), which maps to [0.0, 1.0] in floating pointer
		void setFactorWarped(MixType factor) {
			integerFactor = factor;
		}

		[[nodiscard]]
		MixType mix(MixType v1, MixType v2) const {
			return (v1 * integerFactor + v2 * (shifted - integerFactor)) >> precision;
		}
	};
}
