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

void FftCascade::setParams(Params _params, FFT* _fftPtr, FftCascade* _successorPtr, index _cascadeIndex) {
	// check for unchanged params should be done in FftAnalyzer handler

	params = _params;
	successorPtr = _successorPtr;
	fftPtr = _fftPtr;
	cascadeIndex = _cascadeIndex;

	buffer.reset();
	buffer.setMaxSize(params.fftSize * 5);

	resampleResult();

	const auto cascadeSampleRate = index(params.samplesPerSec / std::pow(2, _cascadeIndex));
	filter.setParams(params.legacy_attackTime, params.legacy_decayTime, cascadeSampleRate, params.inputStride);
}

void FftCascade::process(array_view<float> wave, clock::time_point killTime) {
	if (wave.empty()) {
		return;
	}

	array_span<float> newChunk;

	const bool needDownsample = cascadeIndex != 0;
	if (needDownsample) {
		const auto requiredSize = downsampleHelper.pushData(wave);
		newChunk = buffer.allocateNext(requiredSize);
		downsampleHelper.downsampleFixed<2>(newChunk);
	} else {
		newChunk = buffer.allocateNext(wave.size());
		wave.transferToSpan(newChunk);
	}

	if (successorPtr != nullptr) {
		successorPtr->process(newChunk, killTime);
	}

	if (clock::now() > killTime) {
		while (true) {
			auto chunk = buffer.getFirst(params.fftSize);
			if (chunk.empty()) {
				break;
			}

			params.callback(values, cascadeIndex);

			buffer.removeFirst(params.inputStride);
		}

		return;
	}

	while (true) {
		auto chunk = buffer.getFirst(params.fftSize);
		if (chunk.empty()) {
			break;
		}

		doFft(chunk);
		params.callback(values, cascadeIndex);

		buffer.removeFirst(params.inputStride);
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
			const index oldIndex = inter.toValue(i);
			values[i] = values[oldIndex];
		}
	}

	if (index(values.size()) > newValuesSize) {
		for (index i = 1; i < newValuesSize; i++) {
			const index oldIndex = inter.toValue(i);
			values[i] = values[oldIndex];
		}

		values.resize(newValuesSize);
	}

	values[0] = 0.0;
}

void FftCascade::doFft(array_view<float> chunk) {
	fftPtr->process(chunk);

	const auto binsCount = params.fftSize / 2;

	const auto newDC = float(fftPtr->getDC());
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
			const float newValue = fftPtr->getBinMagnitude(bin);
			values[bin] = filter.apply(oldValue, newValue);
		}
	} else {
		for (index bin = 1; bin < binsCount; ++bin) {
			values[bin] = fftPtr->getBinMagnitude(bin);
		}
	}

	hasChanges = true;
}
