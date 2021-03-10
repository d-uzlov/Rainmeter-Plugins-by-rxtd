// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "RealFft.h"

using rxtd::fft_utils::RealFft;

void RealFft::setParams(index _size, std::vector<float> _window) {
	if (const index windowSize = static_cast<index>(_window.size());
		windowSize != _size && windowSize != 0) {
		throw std::runtime_error{ "FFT::setParams(): invalid window size" };
	}

	size = _size / 2;
	scalar = 1.0f / static_cast<float>(size);
	window = std::move(_window);

	kiss.assign(static_cast<std::size_t>(size), false);

	inputBuffer.resize(static_cast<size_t>(size * 2));
	outputBuffer.resize(static_cast<size_t>(size));
}

void RealFft::fillMagnitudes(array_span<float> result) const {
	for (index i = 0; i < result.size(); ++i) {
		const auto v = outputBuffer[static_cast<size_t>(i)];
		const float square = v.real() * v.real() + v.imag() * v.imag();
		result[i] = std::sqrt(square) * scalar;
	}
}

void RealFft::process(array_view<float> wave) {
	if (!window.empty()) {
		for (index i = 0; i < wave.size(); ++i) {
			inputBuffer[static_cast<size_t>(i)] = wave[i] * window[static_cast<size_t>(i)];
		}
		kiss.transform_real(inputBuffer.data(), outputBuffer.data());
	} else {
		kiss.transform_real(wave.data(), outputBuffer.data());
	}
}
