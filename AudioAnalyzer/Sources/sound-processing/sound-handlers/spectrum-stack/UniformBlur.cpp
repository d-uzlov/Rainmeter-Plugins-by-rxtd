/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "UniformBlur.h"
#include "BandResampler.h"
#include "option-parser/OptionMap.h"

using namespace audio_analyzer;

SoundHandler::ParseResult UniformBlur::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return { };
	}

	//                                                        ?? ↓↓ looks best ?? at 0.25 ↓↓ ??
	params.blurRadius = std::max<double>(om.get(L"Radius").asFloat(1.0) * 0.25, 0.0);
	params.blurRadiusAdaptation = std::max<double>(om.get(L"RadiusAdaptation").asFloat(2.0), 0.0);

	ParseResult result;
	result.setParams(std::move(params));
	result.addSource(sourceId);
	return result;
}

SoundHandler::ConfigurationResult UniformBlur::vConfigure(const std::any& _params, Logger& cl) {
	params = std::any_cast<Params>(_params);

	auto& config = getConfiguration();

	index startingLayer = 0;
	if (const auto provider = dynamic_cast<ResamplerProvider*>(config.sourcePtr);
		provider != nullptr) {
		const BandResampler* resamplerPtr = provider->getResampler();
		if (resamplerPtr != nullptr) {
			startingLayer = resamplerPtr->getStartingLayer();
		}
	}

	startingRadius = params.blurRadius * std::pow(params.blurRadiusAdaptation, startingLayer);

	const auto dataSize = config.sourcePtr->getDataSize();
	return dataSize;
}

void UniformBlur::vProcess(array_view<float> wave, clock::time_point killTime) {
	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;

	double theoreticalRadius = startingRadius;

	const index cascadesCount = source.getDataSize().layersCount;
	for (index i = 0; i < cascadesCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			const auto cascadeSource = chunk.data;
			auto cascadeResult = pushLayer(i, chunk.equivalentWaveSize);

			const index radius = std::llround(theoreticalRadius);
			if (radius < 1) {
				std::copy(cascadeSource.begin(), cascadeSource.end(), cascadeResult.begin());
			} else {
				blurCascade(cascadeSource, cascadeResult, radius);
			}
		}

		theoreticalRadius *= params.blurRadiusAdaptation;
	}
}

void UniformBlur::blurCascade(array_view<float> source, array_span<float> dest, index radius) {
	auto kernel = gcm.forRadius(radius);

	const auto bandsCount = source.size();
	for (index i = 0; i < bandsCount; ++i) {

		index bandStartIndex = i - radius;
		index kernelStartIndex = 0;

		if (bandStartIndex < 0) {
			kernelStartIndex = -bandStartIndex;
			bandStartIndex = 0;
		}

		double result = 0.0;
		index kernelIndex = kernelStartIndex;
		index bandIndex = bandStartIndex;

		while (true) {
			if (bandIndex >= bandsCount || kernelIndex >= index(kernel.size())) {
				break;
			}
			result += kernel[kernelIndex] * source[bandIndex];

			kernelIndex++;
			bandIndex++;
		}

		dest[i] = float(result);
	}
}
