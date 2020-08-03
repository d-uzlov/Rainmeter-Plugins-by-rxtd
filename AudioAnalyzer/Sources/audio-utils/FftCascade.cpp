/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FftCascade.h"

#include "DiscreetInterpolator.h"

using namespace audio_utils;

void FftCascade::setParams(Params _params, FFT* fft, FftCascade* successor, index cascadeIndex) {
	// check for unchanged params should be done in FftAnalyzer handler

	params = _params;
	this->successor = successor;
	this->fft = fft;
	this->cascadeIndex = cascadeIndex;

	buffer.setSize(params.fftSize);

	resampleResult();

	const auto cascadeSampleRate = index(params.samplesPerSec / std::pow(2, cascadeIndex));
	filter.setParams(params.legacy_attackTime, params.legacy_decayTime, cascadeSampleRate, params.inputStride);
}

void FftCascade::process(array_view<float> wave) {
	if (wave.empty()) {
		return;
	}

	const bool needDownsample = cascadeIndex != 0;

	while (!wave.empty()) {
		wave = needDownsample ? buffer.fillResampled(wave) : buffer.fill(wave);

		if (!buffer.isFull()) {
			continue;
		}

		if (successor != nullptr) {
			successor->process(buffer.take());
		}

		doFft();

		buffer.shift(params.inputStride);
	}
}

void FftCascade::resampleResult() {
	const index newValuesSize = params.fftSize / 2;

	if (values.empty()) {
		values.resize(newValuesSize);
		return;
	}

	if (index(values.size()) == newValuesSize) {
		return;
	}

	utils::DiscreetInterpolator inter;
	inter.setParams(0.0, double(newValuesSize - 1), 0, values.size() - 1);

	if (index(values.size()) < newValuesSize) {
		values.resize(newValuesSize);

		for (index i = newValuesSize - 1; i >= 1; i--) {
			const index oldIndex = inter.toValueD(i);
			values[i] = values[oldIndex];
		}
	}

	if (index(values.size()) > newValuesSize) {
		for (index i = 1; i < newValuesSize; i++) {
			const index oldIndex = inter.toValueD(i);
			values[i] = values[oldIndex];
		}

		values.resize(newValuesSize);
	}

	values[0] = 0.0;
}

void FftCascade::doFft() {
	fft->process(buffer.getBuffer());

	const auto binsCount = params.fftSize / 2;

	const auto newDC = float(fft->getDC());
	legacy_dc = filter.apply(legacy_dc, newDC);

	auto legacy_zerothBin = std::abs(newDC);
	if (params.legacy_correctZero) {
		constexpr double root2 = 1.4142135623730950488;
		legacy_zerothBin *= root2;
	}

	values[0] = filter.apply(values[0], legacy_zerothBin);

	if (params.legacy_attackTime != 0.0 || params.legacy_decayTime != 0.0) {
		for (index bin = 1; bin < binsCount; ++bin) {
			const float oldValue = values[bin];
			const float newValue = fft->getBinMagnitude(bin);
			values[bin] = filter.apply(oldValue, newValue);
		}
	} else {
		for (index bin = 1; bin < binsCount; ++bin) {
			values[bin] = fft->getBinMagnitude(bin);
		}
	}

	hasChanges = true;
}

void FftCascade::reset() {
	std::fill(values.begin(), values.end(), 0.0f);
	buffer.reset();
}

array_view<float> FftCascade::RingBuffer::fill(array_view<float> wave) {
	const auto ringBufferRemainingSize = index(buffer.size()) - endOffset;
	const auto copySize = std::min(ringBufferRemainingSize, wave.size());

	std::copy(wave.begin(), wave.begin() + copySize, buffer.begin() + endOffset);
	endOffset += copySize;

	wave.remove_prefix(copySize);
	return wave;
}

array_view<float> FftCascade::RingBuffer::fillResampled(array_view<float> wave) {
	array_span<float> bufferPart = buffer;
	bufferPart.remove_prefix(endOffset);
	auto [waveConsumed, bufferFilled] = downsampleHelper.resample(wave, bufferPart);
	
	endOffset += bufferFilled;
	wave.remove_prefix(waveConsumed);

	return wave;
}

void FftCascade::RingBuffer::shift(index stride) {
	assert(endOffset >= stride);

	std::copy(buffer.begin() + stride, buffer.end(), buffer.begin());
	endOffset -= stride;
	takenOffset -= stride;
}

array_view<float> FftCascade::RingBuffer::take() {
	array_view<float> result = { buffer.data() + takenOffset, endOffset - takenOffset };
	takenOffset = endOffset;
	return result;
}
