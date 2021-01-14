/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "legacy_WeightedBlur.h"

using namespace audio_analyzer;

SoundHandler::ParseResult legacy_WeightedBlur::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	ParseResult result{ true };
	auto& params = result.params.clear<Params>();

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return {};
	}

	//                                                                      ?? ↓↓ looks best ?? at 0.25 ↓↓ ??
	params.radiusMultiplier = std::max<double>(om.get(L"radiusMultiplier").asFloat(1.0) * 0.25, 0.0);

	params.minRadius = std::max<double>(om.get(L"minRadius").asFloat(1.0), 0.0);
	params.maxRadius = std::max<double>(om.get(L"maxRadius").asFloat(20.0), params.minRadius);

	params.minRadiusAdaptation = std::max<double>(om.get(L"MinRadiusAdaptation").asFloat(2.0), 0.0);
	params.maxRadiusAdaptation = std::max<double>(
		om.get(L"MaxRadiusAdaptation").asFloat(params.minRadiusAdaptation), 0.0
	);

	// params.minWeight = std::max<double>(optionMap.get(L"minWeight"sv).asFloat(0), std::numeric_limits<float>::epsilon());
	params.minWeight = 0.0; // doesn't work as expected

	result.sources.emplace_back(sourceId);
	return result;
}

SoundHandler::ConfigurationResult
legacy_WeightedBlur::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	resamplerPtr = getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"invalid source: BandResampler is not found in the handler chain");
		return {};
	}

	auto& config = getConfiguration();
	const auto dataSize = config.sourcePtr->getDataSize();
	return dataSize;
}

void legacy_WeightedBlur::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;
	const BandResampler& resampler = *resamplerPtr;
	const index layersCount = source.getDataSize().layersCount;

	double minRadius = params.minRadius * std::pow(params.minRadiusAdaptation, resampler.getStartingLayer());
	double maxRadius = params.maxRadius * std::pow(params.maxRadiusAdaptation, resampler.getStartingLayer());

	for (index i = 0; i < layersCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			const auto cascadeMagnitudes = chunk;
			const auto cascadeWeights = resampler.getLayerWeights(i);

			auto result = pushLayer(i);
			if (maxRadius <= 1.0 || clock::now() > context.killTime) {
				cascadeMagnitudes.transferToSpan(result);
			} else {
				blurCascade(
					cascadeMagnitudes, cascadeWeights, result,
					std::llround(minRadius), std::llround(maxRadius)
				);
			}
		}

		minRadius *= params.minRadiusAdaptation;
		maxRadius *= params.maxRadiusAdaptation;
	}
}

void legacy_WeightedBlur::blurCascade(
	array_view<float> source, array_view<float> weights, array_span<float> dest,
	index minRadius, index maxRadius
) {
	for (index centralBand = 0; centralBand < source.size(); ++centralBand) {
		const double sigma = params.radiusMultiplier / weights[centralBand];
		index radius = std::clamp<index>(std::lround(sigma * 3.0), minRadius, maxRadius);
		radius = std::min<index>(radius, source.size());

		if (radius < 2) {
			dest[centralBand] = source[centralBand];
			continue;
		}

		auto kernel = gcm.forRadius(radius);

		index bandStartIndex = centralBand - radius;
		index kernelStartIndex = 0;

		if (bandStartIndex < 0) {
			kernelStartIndex = -bandStartIndex;
			bandStartIndex = 0;
		}

		double result = 0.0;
		for (index kernelIndex = kernelStartIndex, bandIndex = bandStartIndex;
		     bandIndex < source.size() && kernelIndex < kernel.size();
		     bandIndex++, kernelIndex++) {
			result += (weights[bandIndex] > params.minWeight ? 1.0 : 0.0) * kernel[kernelIndex] * source[bandIndex];
		}

		dest[centralBand] = float(result);
	}
}
