/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	class MyMath {
	public:
		// TODO remove this pi?
		static const double pi;

		// return approximately a**b
		[[nodiscard]]
		static double fastPow(double a, double b);

		[[nodiscard]]
		static float fastLog2(float val);

		[[nodiscard]]
		static float fastSqrt(float value);

		[[nodiscard]]
		static double db2amplitude(double value);
	};
}
