/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "InfiniteResponseFilter.h"

#include "undef.h"

using namespace audio_utils;

InfiniteResponseFilter::InfiniteResponseFilter(std::vector<double> a, std::vector<double> b) :
	a(std::move(a)),
	b(std::move(b)) {
	state.resize(std::max(this->a.size(), this->b.size()) - 1);
}

void InfiniteResponseFilter::apply(array_span<float> signal) {
	if (a.size() != b.size()) {
		std::terminate(); // TODO
	}

	if (a.empty()) {
		return;
	}

	for (float& value : signal) {
		const double next = value;
		const double nextFiltered = (b[0] * next + state[0]) * a[0];
		value = nextFiltered;
		updateState(next, nextFiltered);
	}
}

void InfiniteResponseFilter::updateState(double next, double nextFiltered) {
	const index lastIndex = state.size() - 1;
	for (index i = 0; i < lastIndex; ++i) {
		const double ai = a[i + 1];
		const double bi = b[i + 1];
		const double prevD = state[i + 1];
		state[i] = bi * next - ai * nextFiltered + prevD;
	}
	state[lastIndex] = b[lastIndex + 1] * next - a[lastIndex + 1] * nextFiltered;
}
