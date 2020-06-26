/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FFT.h"

#include "undef.h"

using namespace audio_utils;

FFT::FFT(index fftSize) {
	setSize(fftSize);
}

void FFT::setSize(index newSize) {
	fftSize = newSize;
	scalar = 1.0 / std::sqrt(fftSize);
	windowFunction = createWindowFunction(fftSize);
	kiss.assign(fftSize / 2, false);

	inputBufferSize = fftSize;
	outputBufferSize = fftSize / 2;
}

double FFT::getDC() const {
	return outputBuffer[0].real() * scalar;
}

double FFT::getBinMagnitude(index binIndex) const {
	const auto &v = outputBuffer[binIndex];
	const auto square = v.real() * v.real() + v.imag() * v.imag();
	// return fastSqrt(square); // doesn't seem to improve performance
	return std::sqrt(square) * scalar;
}

void FFT::setBuffers(input_buffer_type* inputBuffer, output_buffer_type* outputBuffer) {
	this->inputBuffer = inputBuffer;
	this->outputBuffer = outputBuffer;
}

void FFT::process(const float* wave) {
	for (index iBin = 0; iBin < fftSize; ++iBin) {
		inputBuffer[iBin] = wave[iBin] * windowFunction[iBin];
	}

	kiss.transform_real(inputBuffer, outputBuffer);
}

index FFT::getInputBufferSize() const {
	return inputBufferSize;
}

index FFT::getOutputBufferSize() const {
	return outputBufferSize;
}

std::vector<float> FFT::createWindowFunction(index fftSize) {
	std::vector<float> windowFunction;
	windowFunction.resize(fftSize);
	constexpr double _2pi = 2 * 3.14159265358979323846;

	// calculate window function coefficients
	// http://en.wikipedia.org/wiki/Window_function#Hann_.28Hanning.29_window
	for (index bin = 0; bin < fftSize; ++bin) {
		windowFunction[bin] = static_cast<float>(0.5 * (1.0 - std::cos(_2pi * bin / (fftSize - 1))));
	}

	return windowFunction;
}
