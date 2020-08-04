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

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool legacy_FiniteTimeFilter::parseParams(
	const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr
) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
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

	return true;
}

void legacy_FiniteTimeFilter::setParams(const Params& value) {
	params = value;

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
}

SoundHandler::LinkingResult legacy_FiniteTimeFilter::vFinishLinking(Logger& cl) {
	const auto sourcePtr = getSource();
	if (sourcePtr == nullptr) {
		cl.error(L"source is not found");
		return { };
	}

	auto& source = *sourcePtr;

	const auto dataSize = source.getDataSize();

	pastValues.resize(dataSize.layersCount);
	for (auto& vec : pastValues) {
		vec.setBuffersCount(params.smoothingFactor);
		vec.setBufferSize(dataSize.valuesCount);
		vec.init();
	}

	return dataSize;
}

void legacy_FiniteTimeFilter::vProcess(array_view<float> wave) {
	changed = true;
}

void legacy_FiniteTimeFilter::vFinish() {
	if (!changed) {
		return;
	}

	changed = false;

	auto& source = *getSource();

	source.finish();
	const auto sourceData = source.getData();
	const index layersCount = source.getDataSize().layersCount;

	if (params.smoothingFactor <= 1) {
		return;
	}

	pastValuesIndex++;
	if (pastValuesIndex >= params.smoothingFactor) {
		pastValuesIndex = 0;
	}

	for (index layer = 0; layer < layersCount; ++layer) {
		auto& layerPastValues = pastValues[layer];
		const auto sourceValues = sourceData.values[layer];
		std::copy(sourceValues.begin(), sourceValues.end(), layerPastValues[pastValuesIndex].begin());

		const auto dest = generateLayerData(layer);
		applyToLayer(layerPastValues, dest);
	}
}

void legacy_FiniteTimeFilter::vReset() {
	changed = true;
}

void legacy_FiniteTimeFilter::applyToLayer(utils::array2d_view<float> layerPastValues, array_span<float> dest) const {
	auto pastValuesStartIndex = pastValuesIndex + 1;
	if (pastValuesStartIndex >= params.smoothingFactor) {
		pastValuesStartIndex = 0;
	}
	
	switch (params.smoothingCurve) {
	case SmoothingCurve::FLAT:
		for (index band = 0; band < index(dest.size()); ++band) {
			double outValue = 0.0;
			for (index i = 0; i < params.smoothingFactor; ++i) {
				outValue += layerPastValues[i][band];
			}

			dest[band] = float(outValue * smoothingNormConstant);
		}
		break;

	case SmoothingCurve::LINEAR:
		for (index band = 0; band < index(dest.size()); ++band) {
			double outValue = 0.0;
			index valueWeight = 1;

			for (index i = pastValuesStartIndex; i < params.smoothingFactor; ++i) {
				outValue += layerPastValues[i][band] * valueWeight;
				valueWeight++;
			}
			for (index i = 0; i < pastValuesStartIndex; ++i) {
				outValue += layerPastValues[i][band] * valueWeight;
				valueWeight++;
			}

			dest[band] = float(outValue * smoothingNormConstant);
		}
		break;

	case SmoothingCurve::EXPONENTIAL:
		for (index band = 0; band < index(dest.size()); ++band) {
			double outValue = 0.0;
			double smoothingWeight = 0;
			double weight = 1;

			for (index i = pastValuesStartIndex; i < params.smoothingFactor; ++i) {
				outValue += layerPastValues[i][band] * weight;
				smoothingWeight += weight;
				weight *= params.exponentialFactor;
			}
			for (index i = 0; i < pastValuesStartIndex; ++i) {
				outValue += layerPastValues[i][band] * weight;
				smoothingWeight += weight;
				weight *= params.exponentialFactor;
			}

			dest[band] = float(outValue * smoothingNormConstant);
		}
		break;

	default: std::terminate(); // should be unreachable statement
	}
}
