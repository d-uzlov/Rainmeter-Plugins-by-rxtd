/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FiniteTimeFilter.h"
#include <cmath>
#include "FftAnalyzer.h"
#include "FastMath.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<FiniteTimeFilter::Params> FiniteTimeFilter::parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.smoothingFactor = optionMap.get(L"smoothingFactor"sv).asInt(4);
	if (params.smoothingFactor <= 0) {
		cl.warning(L"smoothingFactor should be >= 1 but {} found, assume 1", params.smoothingFactor);
		params.smoothingFactor = 1;
	}

	params.exponentialFactor = 1;
	if (auto smoothingCurveString = optionMap.get(L"smoothingCurve"sv).asIString();
		smoothingCurveString.empty() || smoothingCurveString == L"exponential") {
		params.smoothingCurve = SmoothingCurve::EXPONENTIAL;
		params.exponentialFactor = optionMap.get(L"exponentialFactor").asFloat(1.5);
	} else if (smoothingCurveString == L"flat") {
		params.smoothingCurve = SmoothingCurve::FLAT;
	} else if (smoothingCurveString == L"linear") {
		params.smoothingCurve = SmoothingCurve::LINEAR;
	} else {
		cl.warning(L"smoothingCurve '{}' now recognized, assume 'flat'", smoothingCurveString);
		params.smoothingCurve = SmoothingCurve::FLAT;
	}

	return params;
}

void FiniteTimeFilter::setParams(Params _params) {
	if (this->params == _params) {
		return;
	}

	this->params = std::move(_params);
	valid = false;
	source = nullptr;

	if (params.smoothingFactor <= 1) {
		smoothingNormConstant = 1.0;
	} else {
		switch (params.smoothingCurve) {
		case SmoothingCurve::FLAT:
			smoothingNormConstant = 1.0 / params.smoothingFactor;
			break;

		case SmoothingCurve::LINEAR:
		{
			const index smoothingWeight = params.smoothingFactor * (params.smoothingFactor + 1) / 2;
			smoothingNormConstant = 1.0 / smoothingWeight;
			break;
		}

		case SmoothingCurve::EXPONENTIAL:
		{
			double smoothingWeight = 0;
			double weight = 1;

			for (index i = 0; i < params.smoothingFactor; ++i) {
				smoothingWeight += weight;
				weight *= params.exponentialFactor;
			}

			smoothingNormConstant = 1.0 / smoothingWeight;
			break;
		}

		default: std::terminate();
		}
	}

	valid = true;
}

void FiniteTimeFilter::process(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	changed = true;
}

void FiniteTimeFilter::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void FiniteTimeFilter::finish(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}
	if (!changed) {
		return;
	}

	valid = false;

	source = dataSupplier.getHandler<>(params.sourceId);
	if (source == nullptr) {
		return;
	}

	const auto resamplerProvider = dynamic_cast<const ResamplerProvider*>(source);
	if (resamplerProvider != nullptr) {
		resampler = resamplerProvider->getResampler();
	}

	if (params.smoothingFactor > 1) {
		adjustSize();
		copyValues();
		applyTimeFiltering();
	}

	changed = false;
	valid = true;
}

void FiniteTimeFilter::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

const wchar_t* FiniteTimeFilter::getProp(const isview& prop) const {
	propString.clear();
	return nullptr; // TODO
}

void FiniteTimeFilter::adjustSize() {
	const auto layersCount = source->getLayersCount();
	const auto valuesCount = source->getData(0).size();

	if (layersCount == index(pastValues.size()) && valuesCount == values.getBufferSize()) {
		return;
	}

	pastValues.resize(layersCount);
	for (auto& vec : pastValues) {
		vec.setBuffersCount(params.smoothingFactor);
		vec.setBufferSize(valuesCount);
		vec.init();
	}
	values.setBuffersCount(layersCount);
	values.setBufferSize(valuesCount);

}

void FiniteTimeFilter::reset() {
	changed = true;
}

void FiniteTimeFilter::copyValues() {
	const auto layersCount = source->getLayersCount();
	
	pastValuesIndex++;
	if (pastValuesIndex >= params.smoothingFactor) {
		pastValuesIndex = 0;
	}
	for (layer_t layer = 0; layer < layersCount; ++layer) {
		auto& layerPastValues = pastValues[layer];
		const auto sourceValues = source->getData(layer);
		for (index i = 0; i < sourceValues.size(); ++i) {
			layerPastValues[pastValuesIndex][i] = sourceValues[i];
		}
	}
}

void FiniteTimeFilter::applyTimeFiltering() {
	auto startPastIndex = pastValuesIndex + 1;
	if (startPastIndex >= params.smoothingFactor) {
		startPastIndex = 0;
	}

	const auto layersCount = source->getLayersCount();
	for (index layer = 0; layer < layersCount; ++layer) {
		auto &currentPastValues = pastValues[layer];
		auto currentValues = values[layer];

		switch (params.smoothingCurve) {
		case SmoothingCurve::FLAT:
			for (index band = 0; band < index(currentValues.size()); ++band) {
				double outValue = 0.0;
				for (index i = 0; i < params.smoothingFactor; ++i) {
					outValue += currentPastValues[i][band];
				}

				currentValues[band] = outValue * smoothingNormConstant;
			}
			break;

		case SmoothingCurve::LINEAR:
			for (index band = 0; band < index(currentValues.size()); ++band) {
				double outValue = 0.0;
				index valueWeight = 1;

				for (index i = startPastIndex; i < params.smoothingFactor; ++i) {
					outValue += currentPastValues[i][band] * valueWeight;
					valueWeight++;
				}
				for (index i = 0; i < startPastIndex; ++i) {
					outValue += currentPastValues[i][band] * valueWeight;
					valueWeight++;
				}

				currentValues[band] = outValue * smoothingNormConstant;
			}
			break;

		case SmoothingCurve::EXPONENTIAL:
			for (index band = 0; band < index(currentValues.size()); ++band) {
				double outValue = 0.0;
				double smoothingWeight = 0;
				double weight = 1;

				for (index i = startPastIndex; i < params.smoothingFactor; ++i) {
					outValue += currentPastValues[i][band] * weight;
					smoothingWeight += weight;
					weight *= params.exponentialFactor;
				}
				for (index i = 0; i < startPastIndex; ++i) {
					outValue += currentPastValues[i][band] * weight;
					smoothingWeight += weight;
					weight *= params.exponentialFactor;
				}

				currentValues[band] = outValue * smoothingNormConstant;
			}
			break;

		default: std::terminate(); // should be unreachable statement
		}
	}
}
