/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	// watch for int overflow
	// use 64-bit types when not sure
	// leave some headroom with precision: more precision — more risk of overflow
	template<typename IntType = int_fast32_t, uint8_t precision = 8>
	class IntMixer {
		static_assert(std::is_integral<IntType>::value);
		static_assert(precision <= (sizeof(IntType) * 8 - 2));

		using MixType = IntType;
		MixType fi = 0;
		static constexpr MixType shift = 1 << precision;

	public:
		IntMixer() = default;

		IntMixer(double factor) {
			setFactor(factor);
		}

		IntMixer(float factor) {
			setFactor(factor);
		}

		void setFactor(double factor) {
			fi = MixType(factor * shift);
		}

		void setFactor(float factor) {
			fi = MixType(factor * shift);
		}

		// range is from 0 to (1 << precision), which maps to [0.0, 1.0] in floating pointer
		void setFactorWarped(MixType factor) {
			fi = factor;
		}

		[[nodiscard]]
		MixType mix(MixType v1, MixType v2) const {
			return (v1 * fi + v2 * (shift - fi)) >> precision;
		}
	};
}
