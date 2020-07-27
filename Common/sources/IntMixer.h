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

	public:
		IntMixer() = default;

		IntMixer(double factor) {
			setParams(factor);
		}

		void setParams(double factor) {
			fi = MixType(factor * (1 << precision));
		}

		MixType mix(MixType v1, MixType v2) const {
			return (v1 * fi + v2 * ((1 << precision) - fi)) >> precision;
		}
	};
}
