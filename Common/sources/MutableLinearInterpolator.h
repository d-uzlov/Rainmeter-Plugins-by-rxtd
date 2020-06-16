#pragma once

namespace rxtd::utils {

	class MutableLinearInterpolator {
		double valMin = 0;
		double linMin = 0;
		double alpha = 1;
		double reverseAlpha = 1;

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
		}

		double toValue(double linear) const {
			return valMin + (linear - linMin) * alpha;
		}

		double toLinear(double value) const {
			return linMin + (value - valMin) * reverseAlpha;
		}
	};
}