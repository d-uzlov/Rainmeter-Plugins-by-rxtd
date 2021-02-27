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
	scalar = 1.0f / float(fftSize);
	window = std::move(_window);
	kiss.assign(fftSize / 2, false);

	inputBuffer.resize(fftSize);
	outputBuffer.resize(fftSize / 2);
}

double FFT::getDC() const {
	return outputBuffer[0].real() * scalar;
}

float FFT::getBinMagnitude(index binIndex) const {
	const auto v = outputBuffer[binIndex];
	const float square = v.real() * v.real() + v.imag() * v.imag();
	return std::sqrt(square) * scalar;
}

void FFT::process(array_view<float> wave) {
	for (index i = 0; i < fftSize; ++i) {
		inputBuffer[i] = wave[i] * window[i];
	}

	kiss.transform_real(inputBuffer.data(), outputBuffer.data());
}
