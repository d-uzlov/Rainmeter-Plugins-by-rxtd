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
#include "RandomGenerator.h"

using namespace audio_utils;

static float invalidOddValue = std::numeric_limits<float>::infinity();

void FftCascade::setParams(Params _params, FFT* fft, FftCascade* successor, index cascadeIndex) {
	params = _params;
	this->successor = successor;
	this->fft = fft;

	odd = invalidOddValue;
	buffer.setSize(params.fftSize);

	resampleResult();

	const auto cascadeSampleRate = index(params.samplesPerSec / std::pow(2, cascadeIndex));

	filter.setParams(params.legacy_attackTime, params.legacy_decayTime, cascadeSampleRate, params.inputStride);

	downsampleGain = std::pow(2.0f, cascadeIndex * 0.5f);
}

void FftCascade::process(array_view<float> wave, bool resample) {
	if (wave.empty()) {
		return;
	}

	if (odd != invalidOddValue) {
		buffer.pushOne((odd + wave[0]) * 0.5f);
		odd = invalidOddValue;
		wave.remove_prefix(1);
	}

	while (!wave.empty()) {
		if (resample) {
			wave = buffer.fillResampled(wave);

			if (wave.size() == 1) {
				odd = wave.back();
				wave = { };
			}
		} else {
			wave = buffer.fill(wave);
		}

		if (buffer.isFull()) {
			if (successor != nullptr) {
				successor->process(buffer.take(), true);
			}

			doFft();

			buffer.shift(params.inputStride);
		}
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
	dc = filter.apply(dc, newDC);

	auto zerothBin = std::abs(newDC);
	if (params.correctZero) {
		constexpr double root2 = 1.4142135623730950488;
		zerothBin *= root2;
	}
	zerothBin = zerothBin * downsampleGain;

	values[0] = filter.apply(values[0], zerothBin);

	if (params.legacy_attackTime != 0.0 || params.legacy_decayTime != 0.0) {
		for (index bin = 1; bin < binsCount; ++bin) {
			const float oldValue = values[bin];
			const float newValue = fft->getBinMagnitude(bin) * downsampleGain;
			values[bin] = filter.apply(oldValue, newValue);
		}
	} else {
		for (index bin = 1; bin < binsCount; ++bin) {
			values[bin] = fft->getBinMagnitude(bin) * downsampleGain;
		}
	}
}

void FftCascade::reset() {
	std::fill(values.begin(), values.end(), 0.0f);
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
	const auto ringBufferRemainingSize = index(buffer.size()) - endOffset;
	const auto waveElementPairs = wave.size() / 2;
	const auto copySize = std::min(ringBufferRemainingSize, waveElementPairs);

	for (index i = 0; i < copySize; i++) {
		buffer[endOffset + i] = (wave[i * 2] + wave[i * 2 + 1]) * 0.5f;
	}
	endOffset += copySize;

	wave.remove_prefix(copySize * 2);
	return wave;
}

void FftCascade::RingBuffer::pushOne(float value) {
	buffer[endOffset] = value;
	endOffset++;
}

void FftCascade::RingBuffer::shift(index stride) {
	assert(endOffset >= stride);

	std::copy(buffer.begin() + stride, buffer.end(), buffer.begin());
	endOffset -= stride;
	takenOffset -= stride;
}
