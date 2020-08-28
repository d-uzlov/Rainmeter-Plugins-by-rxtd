/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "legacy_FiniteTimeFilter.h"

using namespace audio_analyzer;

SoundHandler::ParseResult legacy_FiniteTimeFilter::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	params.smoothingFactor = om.get(L"smoothingFactor").asInt(4);
	if (params.smoothingFactor <= 0) {
		cl.warning(L"smoothingFactor should be >= 1 but {} found. Assume 1", params.smoothingFactor);
		params.smoothingFactor = 1;
	}

	params.exponentialFactor = 1.0;
	if (const auto smoothingCurveString = om.get(L"smoothingCurve").asIString(L"exponential");
		smoothingCurveString == L"exponential") {
		params.smoothingCurve = SmoothingCurve::EXPONENTIAL;
		params.exponentialFactor = om.get(L"exponentialFactor").asFloat(1.5);
	} else if (smoothingCurveString == L"flat") {
		params.smoothingCurve = SmoothingCurve::FLAT;
	} else if (smoothingCurveString == L"linear") {
		params.smoothingCurve = SmoothingCurve::LINEAR;
	} else {
		cl.warning(L"smoothingCurve '{}' is not recognized. Assume 'flat'", smoothingCurveString);
		params.smoothingCurve = SmoothingCurve::FLAT;
	}

	ParseResult result{ true };
	result.params = params;
	result.sources.emplace_back(sourceId);
	return result;
}

SoundHandler::ConfigurationResult
legacy_FiniteTimeFilter::vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) {
	params = std::any_cast<Params>(_params);

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
		}
	}

	auto& config = getConfiguration();
	const auto dataSize = config.sourcePtr->getDataSize();

	pastValues.resize(dataSize.layersCount);
	for (auto& vec : pastValues) {
		vec.setBuffersCount(params.smoothingFactor);
		vec.setBufferSize(dataSize.valuesCount);
		vec.fill();
	}

	return dataSize;
}

void legacy_FiniteTimeFilter::vProcess(array_view<float> wave, clock::time_point killTime) {
	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;

	const index layersCount = source.getDataSize().layersCount;
	for (index layer = 0; layer < layersCount; ++layer) {
		for (auto chunk : source.getChunks(layer)) {
			if (params.smoothingFactor <= 1) {
				auto dest = pushLayer(layer, chunk.equivalentWaveSize);
				std::copy(chunk.data.begin(), chunk.data.end(), dest.begin());
				continue;
			}

			pastValuesIndex++;
			if (pastValuesIndex >= params.smoothingFactor) {
				pastValuesIndex = 0;
			}

			auto& layerPastValues = pastValues[layer];

			const auto sourceValues = chunk.data;
			std::copy(sourceValues.begin(), sourceValues.end(), layerPastValues[pastValuesIndex].begin());

			auto dest = pushLayer(layer, chunk.equivalentWaveSize);
			if (clock::now() > killTime) {
				std::copy(sourceValues.begin(), sourceValues.end(), dest.begin());
			} else {
				applyToLayer(layerPastValues, dest);
			}
		}
	}
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
	}
}
