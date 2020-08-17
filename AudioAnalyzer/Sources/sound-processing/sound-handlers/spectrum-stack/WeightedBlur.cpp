/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WeightedBlur.h"
#include "option-parser/OptionMap.h"

using namespace audio_analyzer;

bool WeightedBlur::parseParams(const OptionMap& om, Logger& cl, const Rainmeter& rain, void* paramsPtr,
                               index legacyNumber) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = om.get(L"source").asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	//                                                                      ?? ↓↓ looks best ?? at 0.25 ↓↓ ?? // TODO
	params.radiusMultiplier = std::max<double>(om.get(L"radiusMultiplier").asFloat(1.0) * 0.25, 0.0);

	params.minRadius = std::max<double>(om.get(L"minRadius").asFloat(1.0), 0.0);
	params.maxRadius = std::max<double>(om.get(L"maxRadius").asFloat(20.0), params.minRadius);

	params.minRadiusAdaptation = std::max<double>(om.get(L"MinRadiusAdaptation").asFloat(2.0), 0.0);
	params.maxRadiusAdaptation = std::max<double>(
		om.get(L"MaxRadiusAdaptation").asFloat(params.minRadiusAdaptation), 0.0);

	// params.minWeight = std::max<double>(optionMap.get(L"minWeight"sv).asFloat(0), std::numeric_limits<float>::epsilon());
	params.minWeight = 0.0; // doesn't work as expected

	return true;
}

SoundHandler::LinkingResult WeightedBlur::vFinishLinking(Logger& cl) {
	resamplerPtr = getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"invalid source: BandResampler is not found in the handler chain");
		return { };
	}

	auto& config = getConfiguration();
	const auto dataSize = config.sourcePtr->getDataSize();
	return dataSize;
}

void WeightedBlur::vProcess(array_view<float> wave) {
	changed = true;
}

void WeightedBlur::vFinish() {
	if (!changed) {
		return;
	}

	changed = false;

	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;
	source.finish();
	const BandResampler& resampler = *resamplerPtr;
	const index layersCount = source.getDataSize().layersCount;

	double minRadius = params.minRadius * std::pow(params.minRadiusAdaptation, resampler.getStartingLayer());
	double maxRadius = params.maxRadius * std::pow(params.maxRadiusAdaptation, resampler.getStartingLayer());

	for (index i = 0; i < layersCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			const auto cascadeMagnitudes = chunk.data;
			const auto cascadeWeights = resampler.getLayerWeights(i);

			const auto result = generateLayerData(i, chunk.equivalentWaveSize);
			blurCascade(cascadeMagnitudes, cascadeWeights, result, std::llround(minRadius), std::llround(maxRadius));
		}

		minRadius *= params.minRadiusAdaptation;
		maxRadius *= params.maxRadiusAdaptation;
	}
}

void WeightedBlur::blurCascade(
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
