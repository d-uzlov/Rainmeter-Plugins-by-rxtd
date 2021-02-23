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
	//
	// Neat little class to avoid writing linear interpolation by hand again and again.
	// It is definitely inferior to std::lerp in terms of edge cases
	// but it is probably faster on all other cases and std::lerp is only C++20
	//
	template<typename T = double>
	class LinearInterpolator {
		T valMin = 0;
		T linMin = 0;
		T alpha = 1;
		T reverseAlpha = 1;

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

		friend bool operator==(const LinearInterpolator& lhs, const LinearInterpolator& rhs) {
			return lhs.valMin == rhs.valMin
				&& lhs.linMin == rhs.linMin
				&& lhs.alpha == rhs.alpha;
		}

		friend bool operator!=(const LinearInterpolator& lhs, const LinearInterpolator& rhs) { return !(lhs == rhs); }
	};
};
