/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	template<typename T = double>
	class LinearInterpolator {
		double valMin = 0;
		double linMin = 0;
		double alpha = 1;
		double reverseAlpha = 1;

	public:
		LinearInterpolator() = default;

		LinearInterpolator(T linMin, T linMax, T valMin, T valMax) {
			setParams(linMin, linMax, valMin, valMax);
		}

		void setParams(T linMin, T linMax, T valMin, T valMax) {
			this->linMin = linMin;
			this->valMin = valMin;
			this->alpha = (valMax - valMin) / (linMax - linMin);
			this->reverseAlpha = 1 / alpha;
		}

		[[nodiscard]]
		T toValue(T linear) const {
			return valMin + (linear - linMin) * alpha;
		}

		[[nodiscard]]
		T toLinear(T value) const {
			return linMin + (value - valMin) * reverseAlpha;
		}
	};
};
