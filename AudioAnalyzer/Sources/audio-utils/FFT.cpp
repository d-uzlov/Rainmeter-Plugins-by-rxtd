/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FFT.h"

using namespace audio_utils;

FFT::FFT(index fftSize, bool correctScalar) {
	setSize(fftSize, correctScalar);
}

void FFT::setSize(index newSize, bool correctScalar) {
	fftSize = newSize;
	scalar = correctScalar ? float(1.0f / fftSize) : float(1.0 / std::sqrt(fftSize));
	window = createHannWindow(fftSize);
	kiss.assign(fftSize / 2, false);

	inputBuffer.resize(fftSize);
	outputBuffer.resize(fftSize / 2);
}

double FFT::getDC() const {
	return outputBuffer[0].real() * scalar;
}

float FFT::getBinMagnitude(index binIndex) const {
	const auto& v = outputBuffer[binIndex];
	const auto square = v.real() * v.real() + v.imag() * v.imag();
	// return fastSqrt(square); // doesn't seem to improve performance
	return std::sqrt(square) * scalar;
}

void FFT::process(array_view<float> wave) {
	for (index iBin = 0; iBin < fftSize; ++iBin) {
		inputBuffer[iBin] = wave[iBin] * window[iBin];
	}

	kiss.transform_real(inputBuffer.data(), outputBuffer.data());
}

std::vector<float> FFT::createHannWindow(index fftSize) {
	std::vector<float> window;
	window.resize(fftSize);
	constexpr double _2pi = 2 * 3.14159265358979323846;

	// http://en.wikipedia.org/wiki/Window_function#Hann_.28Hanning.29_window
	for (index bin = 0; bin < fftSize; ++bin) {
		window[bin] = static_cast<float>(0.5 * (1.0 - std::cos(_2pi * bin / (fftSize - 1))));
	}

	return window;
}
