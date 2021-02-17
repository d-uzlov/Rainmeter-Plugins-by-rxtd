/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "InfiniteResponseFilter.h"

using namespace rxtd::audio_utils;

InfiniteResponseFilter::InfiniteResponseFilter(std::vector<double> a, std::vector<double> b, double gainAmp) {
	if (a.empty()) {
		throw std::exception{};
	}

	this->gainAmp = gainAmp;
	const double a0 = a[0];
	for (auto& value : a) {
		value /= a0;
	}
	for (auto& value : b) {
		value /= a0;
	}

	const index maxSize = std::max(a.size(), b.size());
	a.resize(maxSize);
	b.resize(maxSize);

	state.resize(maxSize - 1);

	this->a = std::move(a);
	this->b = std::move(b);
}

double InfiniteResponseFilter::updateState(double value) {
	const double filtered = b[0] * value + state[0];

	const index lastIndex = state.size() - 1;
	for (index i = 0; i < lastIndex; ++i) {
		const double ai = a[i + 1];
		const double bi = b[i + 1];
		const double prevState = state[i + 1];
		state[i] = bi * value - ai * filtered + prevState;
	}

	state[lastIndex] = b[lastIndex + 1] * value - a[lastIndex + 1] * filtered;

	return filtered;
}
