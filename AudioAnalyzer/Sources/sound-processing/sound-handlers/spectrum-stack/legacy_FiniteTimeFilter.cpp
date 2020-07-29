/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "legacy_FiniteTimeFilter.h"
#include "option-parser/OptionMap.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<legacy_FiniteTimeFilter::Params>
legacy_FiniteTimeFilter::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.smoothingFactor = optionMap.get(L"smoothingFactor"sv).asInt(4);
	if (params.smoothingFactor <= 0) {
		cl.warning(L"smoothingFactor should be >= 1 but {} found. Assume 1", params.smoothingFactor);
		params.smoothingFactor = 1;
	}

	params.exponentialFactor = 1;
	if (const auto smoothingCurveString = optionMap.get(L"smoothingCurve"sv).asIString(L"exponential");
		smoothingCurveString == L"exponential") {
		params.smoothingCurve = SmoothingCurve::EXPONENTIAL;
		params.exponentialFactor = optionMap.get(L"exponentialFactor").asFloat(1.5);
	} else if (smoothingCurveString == L"flat") {
		params.smoothingCurve = SmoothingCurve::FLAT;
	} else if (smoothingCurveString == L"linear") {
		params.smoothingCurve = SmoothingCurve::LINEAR;
	} else {
		cl.warning(L"smoothingCurve '{}' now recognized. Assume 'flat'", smoothingCurveString);
		params.smoothingCurve = SmoothingCurve::FLAT;
	}

	return params;
}

void legacy_FiniteTimeFilter::setParams(const Params& _params, Channel channel) {
	if (params == _params) {
		return;
	}

	params = _params;
	setResamplerID(params.sourceId);

	setValid(false);
	source = nullptr;

	if (params.smoothingFactor <= 1) {
		smoothingNormConstant = 1.0;
	} else {
		switch (params.smoothingCurve) {
		case SmoothingCurve::FLAT:
			smoothingNormConstant = 1.0 / params.smoothingFactor;
			break;

		case SmoothingCurve::LINEAR: {
			const index smoothingWeight = params.smoothingFactor * (params.smoothingFactor + 1) / 2;
			smoothingNormConstant = 1.0 / smoothingWeight;
			break;
		}

		case SmoothingCurve::EXPONENTIAL: {
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

	setValid(true);
}

void legacy_FiniteTimeFilter::_process(const DataSupplier& dataSupplier) {
	source = dataSupplier.getHandler<>(params.sourceId);
	if (source == nullptr) {
		setValid(false);
		return;
	}

	changed = true;
}

void legacy_FiniteTimeFilter::_finish() {
	if (!changed) {
		return;
	}

	setValid(false);

	if (params.smoothingFactor > 1) {
		adjustSize();
		copyValues();
		applyTimeFiltering();
	}

	changed = false;
	setValid(true);
}

void legacy_FiniteTimeFilter::setSamplesPerSec(index value) {
	samplesPerSec = value;
}

void legacy_FiniteTimeFilter::adjustSize() {
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

void legacy_FiniteTimeFilter::reset() {
	changed = true;
}

void legacy_FiniteTimeFilter::copyValues() {
	const auto layersCount = source->getLayersCount();

	pastValuesIndex++;
	if (pastValuesIndex >= params.smoothingFactor) {
		pastValuesIndex = 0;
	}
	for (index layer = 0; layer < layersCount; ++layer) {
		auto& layerPastValues = pastValues[layer];
		const auto sourceValues = source->getData(layer);
		for (index i = 0; i < sourceValues.size(); ++i) {
			layerPastValues[pastValuesIndex][i] = sourceValues[i];
		}
	}
}

void legacy_FiniteTimeFilter::applyTimeFiltering() {
	auto startPastIndex = pastValuesIndex + 1;
	if (startPastIndex >= params.smoothingFactor) {
		startPastIndex = 0;
	}

	const auto layersCount = source->getLayersCount();
	for (index layer = 0; layer < layersCount; ++layer) {
		auto& currentPastValues = pastValues[layer];
		auto currentValues = values[layer];

		switch (params.smoothingCurve) {
		case SmoothingCurve::FLAT:
			for (index band = 0; band < index(currentValues.size()); ++band) {
				double outValue = 0.0;
				for (index i = 0; i < params.smoothingFactor; ++i) {
					outValue += currentPastValues[i][band];
				}

				currentValues[band] = float(outValue * smoothingNormConstant);
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

				currentValues[band] = float(outValue * smoothingNormConstant);
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

				currentValues[band] = float(outValue * smoothingNormConstant);
			}
			break;

		default: std::terminate(); // should be unreachable statement
		}
	}
}
