/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FftCascade.h"
#include "RandomGenerator.h"

#include "undef.h"

using namespace audio_utils;

void FftCascade::setParams(Params _params, FFT* fft, FftCascade* successor, layer_t cascadeIndex) {
	params = _params;
	this->successor = successor;
	this->fft = fft;

	filledElements = 0;
	transferredElements = 0;
	ringBuffer.resize(params.fftSize);

	const index newValuesSize = params.fftSize / 2;
	if (values.empty()) {
		values.resize(newValuesSize);
	} else if (index(values.size()) != newValuesSize) {
		double coef = double(values.size()) / newValuesSize;

		if (index(values.size()) < newValuesSize) {
			values.resize(newValuesSize);

			for (int i = newValuesSize - 1; i >= 1; i--) {
				index oldIndex = std::clamp<index>(i * coef, 0, values.size() - 1);
				values[i] = values[oldIndex];
			}
		}

		if (index(values.size()) > newValuesSize) {
			for (int i = 1; i < newValuesSize; i++) {
				index oldIndex = std::clamp<index>(i * coef, 0, values.size() - 1);
				values[i] = values[oldIndex];
			}

			values.resize(newValuesSize);
		}

		values[0] = 0.0;
		// std::fill(values.begin(), values.end(), 0.0f);
	}

	auto samplesPerSec = params.samplesPerSec;
	samplesPerSec = index(samplesPerSec / std::pow(2, cascadeIndex));

	filter.setParams(params.legacy_attackTime, params.legacy_decayTime, samplesPerSec, params.inputStride);

	downsampleGain = std::pow(2, cascadeIndex * 0.5);
}

void FftCascade::process(array_view<float> wave) {
	const auto fftSize = params.fftSize;
	const auto inputStride = params.inputStride;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != wave.size()) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= wave.size()) {
			const auto copyStartPlace = wave.data() + waveProcessed;
			std::copy(copyStartPlace, copyStartPlace + copySize, tmpIn + filledElements);

			if (successor != nullptr) {
				successor->processResampled({ tmpIn + transferredElements, index(ringBuffer.size()) - transferredElements });
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			std::copy(wave.data() + waveProcessed, wave.data() + wave.size(), tmpIn + filledElements);

			filledElements = filledElements + wave.size() - waveProcessed;
			waveProcessed = wave.size();
		}
	}
}

void FftCascade::processResampled(array_view<float> wave) {
	const auto fftSize = params.fftSize;
	const auto inputStride = params.inputStride;

	if (wave.empty()) {
		return;
	}

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();
	if (std::abs(odd) <= 1.0f) {
		tmpIn[filledElements] = (odd + wave[0]) * 0.5f;
		odd = 10.0f;
		filledElements++;
		waveProcessed = 1;
	}

	while (waveProcessed != wave.size()) {
		const auto elementPairsNeeded = fftSize - filledElements;
		const auto elementPairsLeft = (wave.size() - waveProcessed) / 2;
		const auto copyStartPlace = wave.data() + waveProcessed;

		if (elementPairsNeeded <= elementPairsLeft) {
			for (index i = 0; i < elementPairsNeeded; i++) {
				tmpIn[filledElements + i] = (copyStartPlace[i * 2] + copyStartPlace[i * 2 + 1]) * 0.5f;
			}

			if (successor != nullptr) {
				successor->processResampled({ tmpIn + transferredElements, index(ringBuffer.size()) - transferredElements });
			}

			waveProcessed += elementPairsNeeded * 2;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			for (index i = 0; i < elementPairsLeft; i++) {
				tmpIn[filledElements + i] = (copyStartPlace[i * 2] + copyStartPlace[i * 2 + 1]) * 0.5f;
			}

			filledElements = filledElements + elementPairsLeft;
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

void FftCascade::processSilence(index waveSize) {
	const auto fftSize = params.fftSize;
	const auto inputStride = params.inputStride;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != waveSize) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= waveSize) {
			std::fill_n(tmpIn + filledElements, copySize, 0.0f);

			if (successor != nullptr) {
				successor->processResampled({ tmpIn + transferredElements, index(ringBuffer.size()) - transferredElements });
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			std::fill_n(tmpIn + filledElements, waveSize - waveProcessed, 0.0f);

			filledElements = filledElements + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}
}

void FftCascade::doFft() {
	fft->process(ringBuffer);

	const auto binsCount = params.fftSize / 2;

	const auto newDC = fft->getDC();
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
			const double oldValue = values[bin];
			const double newValue = fft->getBinMagnitude(bin) * downsampleGain;
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
