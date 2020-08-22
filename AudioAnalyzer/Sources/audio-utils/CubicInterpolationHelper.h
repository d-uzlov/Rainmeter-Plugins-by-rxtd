/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"

namespace rxtd::audio_utils {
	class CubicInterpolationHelper {
		array_view<float> sourceValues;

		struct {
			double a;
			double b;
			double c;
			double d;
		} coefs{ };

		index currentIntervalIndex{ };

	public:
		void setSource(array_view<float> value) {
			sourceValues = value;
			currentIntervalIndex = -1;
		}

		[[nodiscard]]
		double getValueFor(double x);

	private:
		[[nodiscard]]
		double calcDerivativeFor(index ind) const;

		// ind should be < sourceValues.size() - 1
		void calcCoefsFor(index ind);
	};
}
