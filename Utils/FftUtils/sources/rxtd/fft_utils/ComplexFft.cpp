// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "ComplexFft.h"

using rxtd::fft_utils::ComplexFft;

void ComplexFft::setParams(index _size, bool reverse) {
	size = _size;
	scalar = reverse ? 1.0f : 1.0f / static_cast<float>(size);

	kiss = { static_cast<std::size_t>(size), reverse };

	outputBuffer.resize(static_cast<size_t>(size));
}

void ComplexFft::process(array_view<complex_type> input) {
	kiss.transform(input.data(), outputBuffer.data());
	for (auto& val : outputBuffer) {
		val *= scalar;
	}
}
