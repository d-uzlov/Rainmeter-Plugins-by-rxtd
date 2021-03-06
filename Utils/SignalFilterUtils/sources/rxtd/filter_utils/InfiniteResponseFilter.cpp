// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "InfiniteResponseFilter.h"

using rxtd::filter_utils::InfiniteResponseFilter;

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

	const auto maxSize = std::max(a.size(), b.size());
	a.resize(maxSize);
	b.resize(maxSize);

	state.resize(maxSize - 1);

	this->a = std::move(a);
	this->b = std::move(b);
}

double InfiniteResponseFilter::updateState(double value) {
	const double filtered = b[0] * value + state[0];

	const auto lastIndex = state.size() - 1;
	for (index i = 0; i < static_cast<index>(lastIndex); ++i) {
		auto i2 = static_cast<std::vector<float>::size_type>(i);
		const double ai = a[i2 + 1];
		const double bi = b[i2 + 1];
		const double prevState = state[i2 + 1];
		state[i2] = bi * value - ai * filtered + prevState;
	}

	state[lastIndex] = b[lastIndex + 1] * value - a[lastIndex + 1] * filtered;

	return filtered;
}
