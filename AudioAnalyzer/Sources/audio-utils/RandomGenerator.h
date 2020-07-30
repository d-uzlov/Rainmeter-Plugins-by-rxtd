/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <random>

namespace rxtd::audio_utils {
	class RandomGenerator {
		std::random_device rd;
		std::mt19937 e2;
		std::uniform_real_distribution<> dist;

	public:
		RandomGenerator() : e2(rd()), dist(-1.0, 1.0) {
		}

		[[nodiscard]]
		double next() {
			return dist(e2);
		}
	};
}
