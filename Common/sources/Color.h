/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <algorithm>

namespace rxu {
	class Color {
		double red { }, green { }, blue { }, alpha = 1.0;

	public:
		Color() = default;

		Color(double red, double green, double blue, double alpha) :
			red(red),
			green(green),
			blue(blue),
			alpha(alpha) { }

		Color operator*(double value) const {
			return { red * value, green * value, blue * value, alpha * value };
		}
		Color operator+(const Color &other) const {
			return { red + other.red, green + other.green, blue + other.blue, alpha + other.alpha };
		}

		uint32_t toInt() const {
			const uint8_t r = uint8_t(std::clamp(red * 255, 0.0, 255.0));
			const uint8_t g = uint8_t(std::clamp(green * 255, 0.0, 255.0));
			const uint8_t b = uint8_t(std::clamp(blue * 255, 0.0, 255.0));
			const uint8_t a = uint8_t(std::clamp(alpha * 255, 0.0, 255.0));

			return a << 24 | r << 16 | g << 8 | b;
		}
	};

}
