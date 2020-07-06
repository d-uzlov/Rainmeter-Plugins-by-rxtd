#pragma once

namespace rxtd::utils {
	class MutableLinearInterpolator {
		double valMin = 0;
		double linMin = 0;
		double alpha = 1;
		double reverseAlpha = 1;

		index valMinClamp = 0;
		index valMaxClamp = 1;

	public:
		MutableLinearInterpolator() = default;

		MutableLinearInterpolator(double linMin, double linMax, double valMin, double valMax) {
			setParams(linMin, linMax, valMin, valMax);
		}

		void setParams(double linMin, double linMax, double valMin, double valMax) {
			this->linMin = linMin;
			this->valMin = valMin;
			this->alpha = (valMax - valMin) / (linMax - linMin);
			this->reverseAlpha = 1 / alpha;

			this->valMinClamp = std::llround(valMin);
			this->valMaxClamp = std::llround(valMax);

			this->valMinClamp = std::min(this->valMinClamp, this->valMaxClamp);
			this->valMaxClamp = std::max(this->valMinClamp, this->valMaxClamp);
		}

		double toValue(double linear) const {
			return valMin + (linear - linMin) * alpha;
		}

		// Values are clamped to [valMin, valMax] range
		index toValueDiscrete(double linear) const {
			const double value = toValue(linear);
			const index discreetValue = std::llround(value);
			return std::clamp(discreetValue, valMinClamp, valMaxClamp);
		}

		double toLinear(double value) const {
			return linMin + (value - valMin) * reverseAlpha;
		}
	};
}
