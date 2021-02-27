/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FftCascade.h"

#include "rxtd/DiscreetInterpolator.h"

using rxtd::fft_utils::FftCascade;

void FftCascade::setParams(Params _params, FFT* _fftPtr, FftCascade* _successorPtr, index _cascadeIndex) {
	// check for unchanged params should be done in FftAnalyzer handler

	params = _params;
	successorPtr = _successorPtr;
	fftPtr = _fftPtr;
	cascadeIndex = _cascadeIndex;

	buffer.reset();
	buffer.setMaxSize(params.fftSize * 5);

	resampleResult();
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
		values.resize(static_cast<size_t>(newValuesSize));
		return;
	}

	if (static_cast<index>(values.size()) == newValuesSize) {
		return;
	}

	DiscreetInterpolator inter;
	inter.setParams(0.0, static_cast<double>(newValuesSize - 1), 0, static_cast<index>(values.size()) - 1);

	if (static_cast<index>(values.size()) < newValuesSize) {
		values.resize(static_cast<size_t>(newValuesSize));
		array_span<float> valuesView = values;

		for (index i = newValuesSize - 1; i >= 1; i--) {
			const index oldIndex = inter.toValue(i);
			valuesView[i] = valuesView[oldIndex];
		}
	} else {
		array_span<float> valuesView = values;
		for (index i = 1; i < newValuesSize; i++) {
			const index oldIndex = inter.toValue(i);
			valuesView[i] = valuesView[oldIndex];
		}

		values.resize(static_cast<size_t>(newValuesSize));
	}

	values[0] = 0.0;
}

void FftCascade::doFft(array_view<float> chunk) {
	fftPtr->process(chunk);

	const auto binsCount = params.fftSize / 2;

	values[0] = std::abs(fftPtr->getDC());

	for (index bin = 1; bin < binsCount; ++bin) {
		values[static_cast<size_t>(bin)] = fftPtr->getBinMagnitude(bin);
	}

	hasChanges = true;
}
