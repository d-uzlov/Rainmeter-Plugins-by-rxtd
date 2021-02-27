/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FFT.h"

using rxtd::fft_utils::FFT;

void FFT::setParams(index newSize, std::vector<float> _window) {
	fftSize = newSize;
	scalar = 1.0f / static_cast<float>(fftSize);
	window = std::move(_window);
	kiss.assign(static_cast<std::size_t>(fftSize / 2), false);

	inputBuffer.resize(static_cast<size_t>(fftSize));
	outputBuffer.resize(static_cast<size_t>(fftSize / 2));
}

float FFT::getDC() const {
	return outputBuffer[0].real() * scalar;
}

float FFT::getBinMagnitude(index binIndex) const {
	const auto v = outputBuffer[static_cast<size_t>(binIndex)];
	const float square = v.real() * v.real() + v.imag() * v.imag();
	return std::sqrt(square) * scalar;
}

void FFT::process(array_view<float> wave) {
	for (index i = 0; i < fftSize; ++i) {
		inputBuffer[static_cast<size_t>(i)] = wave[i] * window[static_cast<size_t>(i)];
	}

	kiss.transform_real(inputBuffer.data(), outputBuffer.data());
}
