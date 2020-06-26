/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"

namespace rxtd::audio_utils {
	class InfiniteResponseFilter {
		// inspired by https://github.com/BrechtDeMan/loudness.py/blob/master/loudness.py

		std::vector<double> a;
		std::vector<double> b;
		std::vector<double> state;

	public:
		InfiniteResponseFilter() = default;
		InfiniteResponseFilter(std::vector<double> a, std::vector<double> b);

		void apply(array_span<float> signal);

		void updateState(double next, double nextFiltered);
	};

}
