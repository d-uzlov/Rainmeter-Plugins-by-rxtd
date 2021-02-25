#pragma once
#include "LinearInterpolator.h"

namespace rxtd {
	//
	// Handy little class like LinearInterpolator but for discreet values.
	// Main purpose is to sample array indices, so in clamps its values to [min, max] range.
	//
	class DiscreetInterpolator {
		LinearInterpolator<double> li;

		index valMin = 0;
		index valMax = 1;

	public:
		DiscreetInterpolator() = default;

		DiscreetInterpolator(double linMin, double linMax, double valMin, double valMax) = delete;

		DiscreetInterpolator(double linMin, double linMax, index valMin, index valMax) {
			setParams(linMin, linMax, valMin, valMax);
		}

		void setParams(double linMin, double linMax, index valMin, index valMax) {
			li.setParams(
				linMin, linMax + double(std::numeric_limits<float>::epsilon()),
				double(valMin),
				double(valMax + 1) // +1 is counteracted by std::floor
			);

			this->valMin = std::min(valMin, valMax);
			this->valMax = std::max(valMin, valMax);
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index toValue(double linear) const {
			return std::clamp(
				static_cast<index>(std::floor(li.toValue(linear))),
				valMin,
				valMax
			);
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index toValue(index linear) const {
			return toValue(double(linear));
		}
	};
}
