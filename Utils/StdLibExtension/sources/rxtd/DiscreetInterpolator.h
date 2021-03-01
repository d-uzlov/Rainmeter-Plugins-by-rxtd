#pragma once
#include "LinearInterpolator.h"

namespace rxtd {
	//
	// Handy little class like LinearInterpolator but for discreet values.
	// Main purpose is to sample array indices, so in clamps its values to [min, max] range.
	//
	template<typename IntermediateType = double>
	class DiscreetInterpolator {
		LinearInterpolator<IntermediateType> li;

		index valMin = 0;
		index valMax = 1;

	public:
		DiscreetInterpolator() = default;

		DiscreetInterpolator(IntermediateType linMin, IntermediateType linMax, IntermediateType valMin, IntermediateType valMax) = delete;

		DiscreetInterpolator(IntermediateType linMin, IntermediateType linMax, index valMin, index valMax) {
			setParams(linMin, linMax, valMin, valMax);
		}

		void setParams(IntermediateType linMin, IntermediateType linMax, index valMin, index valMax) {
			li.setParams(
				linMin, linMax + static_cast<IntermediateType>(std::numeric_limits<float>::epsilon()),
				static_cast<IntermediateType>(valMin),
				static_cast<IntermediateType>(valMax + 1) // +1 is counteracted by std::floor
			);

			this->valMin = std::min(valMin, valMax);
			this->valMax = std::max(valMin, valMax);
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index toValue(IntermediateType linear) const {
			return std::clamp(
				static_cast<index>(std::floor(li.toValue(linear))),
				valMin,
				valMax
			);
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index toValue(index linear) const {
			return toValue(static_cast<IntermediateType>(linear));
		}
	};
}
