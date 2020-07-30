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

void FftCascade::setParams(Params _params, FFT* fft, FftCascade* successor, index cascadeIndex) {
	params = _params;
	this->successor = successor;
	this->fft = fft;

	ringBufferOffset = 0;
	transferredElements = 0;
	ringBuffer.resize(params.fftSize);

	const index newValuesSize = params.fftSize / 2;
	if (values.empty()) {
		values.resize(newValuesSize);
	} else if (index(values.size()) != newValuesSize) {
		utils::DiscreetInterpolator inter;
		inter.setParams(0.0, double(newValuesSize - 1), 0, values.size() - 1);
		// double coef = double(values.size()) / newValuesSize;

		if (index(values.size()) < newValuesSize) {
			values.resize(newValuesSize);

			for (index i = newValuesSize - 1; i >= 1; i--) {
				// const index oldIndex = std::clamp<index>(index(i * coef ), 0, values.size() - 1);
				const index oldIndex = inter.toValueD(i);
				values[i] = values[oldIndex];
			}
		}

		if (index(values.size()) > newValuesSize) {
			for (index i = 1; i < newValuesSize; i++) {
				// const index oldIndex = std::clamp<index>(index(i * coef), 0, values.size() - 1);
				const index oldIndex = inter.toValueD(i);
				values[i] = values[oldIndex];
			}

			values.resize(newValuesSize);
		}

		values[0] = 0.0;
	}

	auto samplesPerSec = params.samplesPerSec;
	samplesPerSec = index(samplesPerSec / std::pow(2, cascadeIndex));

	filter.setParams(params.legacy_attackTime, params.legacy_decayTime, samplesPerSec, params.inputStride);

	downsampleGain = std::pow(2.0f, cascadeIndex * 0.5f);
}

void FftCascade::process(array_view<float> wave) {
	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != wave.size()) {
		const auto copySize = params.fftSize - ringBufferOffset;

		if (waveProcessed + copySize <= wave.size()) {
			const auto copyStartPlace = wave.data() + waveProcessed;
			std::copy(copyStartPlace, copyStartPlace + copySize, tmpIn + ringBufferOffset);

			if (successor != nullptr) {
				array_view<float> subBuffer = ringBuffer;
				subBuffer.remove_prefix(transferredElements);
				successor->processResampled(subBuffer);
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + params.inputStride, tmpIn + params.fftSize, tmpIn);
			ringBufferOffset = params.fftSize - params.inputStride;
			transferredElements = ringBufferOffset;
		} else {
			std::copy(wave.data() + waveProcessed, wave.data() + wave.size(), tmpIn + ringBufferOffset);

			ringBufferOffset = ringBufferOffset + wave.size() - waveProcessed;
			waveProcessed = wave.size();
		}
	}
}

void FftCascade::processResampled(array_view<float> wave) {
	if (wave.empty()) {
		return;
	}

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();
	if (std::abs(odd) <= 1.0f) {
		tmpIn[ringBufferOffset] = (odd + wave[0]) * 0.5f;
		odd = 10.0f;
		ringBufferOffset++;
		waveProcessed = 1;
	}

	while (waveProcessed != wave.size()) {
		const auto ringBufferRemainingSize = params.fftSize - ringBufferOffset;
		const auto elementPairsLeft = (wave.size() - waveProcessed) / 2;
		const auto copyStartPlace = wave.data() + waveProcessed;

		if (ringBufferRemainingSize <= elementPairsLeft) {
			for (index i = 0; i < ringBufferRemainingSize; i++) {
				tmpIn[ringBufferOffset + i] = (copyStartPlace[i * 2] + copyStartPlace[i * 2 + 1]) * 0.5f;
			}

			if (successor != nullptr) {
				array_view<float> subBuffer = ringBuffer;
				subBuffer.remove_prefix(transferredElements);
				successor->processResampled(subBuffer);
			}

			waveProcessed += ringBufferRemainingSize * 2;

			doFft();
			std::copy(tmpIn + params.inputStride, tmpIn + params.fftSize, tmpIn);
			ringBufferOffset = params.fftSize - params.inputStride;
			transferredElements = ringBufferOffset;
		} else {
			for (index i = 0; i < elementPairsLeft; i++) {
				tmpIn[ringBufferOffset + i] = (copyStartPlace[i * 2] + copyStartPlace[i * 2 + 1]) * 0.5f;
			}

			ringBufferOffset = ringBufferOffset + elementPairsLeft;
			waveProcessed += elementPairsLeft * 2;

			if (waveProcessed != wave.size()) {
				// we should have exactly one element left
				odd = wave[waveProcessed];
				waveProcessed = wave.size();
			} else {
				odd = 10.0f;
			}
		}
	}
}

void FftCascade::doFft() {
	fft->process(ringBuffer);

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
