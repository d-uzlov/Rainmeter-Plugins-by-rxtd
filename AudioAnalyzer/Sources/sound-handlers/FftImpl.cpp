/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FftImpl.h"
#include <cmath>

#include "undef.h"

using namespace audio_analyzer;

struct CountPair {
	index count = 0;
	std::unique_ptr<FftImpl> ptr;
};
static std::map<index, CountPair> cache;
static FftImpl zeroFft(0);

FftImpl* FftImpl::change(FftImpl* old, index newSize) {
	if (old != nullptr && old->fftSize == newSize) {
		return old;
	}
	if (old != nullptr) {
		const auto iter = cache.find(old->fftSize);
		if (iter != cache.end()) {
			iter->second.count--;
			if (iter->second.count == 0) {
				cache.erase(iter);
			}
		}
	}
	if (newSize == 0) {
		return &zeroFft;
	}

	CountPair &pair = cache[newSize];
	pair.count++;
	if (pair.ptr == nullptr) {
		pair.ptr = std::make_unique<FftImpl>(newSize);
	}
	return pair.ptr.get();
}

FftImpl::FftImpl(index fftSize) :
	fftSize(fftSize),
	scalar(1.0 / std::sqrt(fftSize)),
	windowFunction(createWindowFunction(fftSize)),
	kiss(fftSize / 2, false) {

	inputBufferSize = fftSize;
	outputBufferSize = fftSize / 2;
}

double FftImpl::getDC() const {
	return outputBuffer[0].real() * scalar;
}

double FftImpl::getBinMagnitude(index binIndex) const {
	const auto &v = outputBuffer[binIndex];
	const auto square = v.real() * v.real() + v.imag() * v.imag();
	// return fastSqrt(square); // doesn't seem to improve performance
	return std::sqrt(square) * scalar;
}

void FftImpl::setBuffers(input_buffer_type* inputBuffer, output_buffer_type* outputBuffer) {
	this->inputBuffer = inputBuffer;
	this->outputBuffer = outputBuffer;
}

void FftImpl::process(const float* wave) {
	for (index iBin = 0; iBin < fftSize; ++iBin) {
		inputBuffer[iBin] = wave[iBin] * windowFunction[iBin];
	}

	kiss.transform_real(inputBuffer, outputBuffer);
}

index FftImpl::getInputBufferSize() const {
	return inputBufferSize;
}

index FftImpl::getOutputBufferSize() const {
	return outputBufferSize;
}

std::vector<float> FftImpl::createWindowFunction(index fftSize) {
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
