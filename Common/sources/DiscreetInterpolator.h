#pragma once
#include "LinearInterpolator.h"

namespace rxtd::utils {
	class DiscreetInterpolator {
		LinearInterpolator li;

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
				linMin, linMax + std::numeric_limits<float>::epsilon(),
				double(valMin),
				double(valMax + 1) // +1 is counteracted by std::floor in #makeDiscreet
			);

			this->valMin = std::min(valMin, valMax);
			this->valMax = std::max(valMin, valMax);
		}

		// Values are not clamped
		[[nodiscard]]
		double toValue(double linear) const {
			return li.toValue(linear);
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index toValueD(double linear) const {
			return makeDiscreetClamped(toValue(linear));
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index toValueD(index linear) const {
			return toValueD(double(linear));
		}

		// Values are not clamped
		[[nodiscard]]
		index makeDiscreet(double value) const {
			return static_cast<index>(std::floor(value));
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index makeDiscreetClamped(double value) const {
			return clamp(makeDiscreet(value));
		}

		// Values are clamped to [valMin, valMax] range
		[[nodiscard]]
		index clamp(index value) const {
			return std::clamp(value, valMin, valMax);
		}

		[[nodiscard]]
		double percentRelativeToNext(double value) const {
			return value - makeDiscreet(value);
		}
	};
}
