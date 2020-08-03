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

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool WeightedBlur::parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	//                                                                      ?? ↓↓ looks best ?? at 0.25 ↓↓ ?? // TODO
	params.radiusMultiplier = std::max<double>(optionMap.get(L"radiusMultiplier"sv).asFloat(1.0) * 0.25, 0.0);

	params.minRadius = std::max<double>(optionMap.get(L"minRadius"sv).asFloat(1.0), 0.0);
	params.maxRadius = std::max<double>(optionMap.get(L"maxRadius"sv).asFloat(20.0), params.minRadius);

	params.minRadiusAdaptation = std::max<double>(optionMap.get(L"MinRadiusAdaptation"sv).asFloat(2.0), 0.0);
	params.maxRadiusAdaptation = std::max<double>(
		optionMap.get(L"MaxRadiusAdaptation"sv).asFloat(params.minRadiusAdaptation), 0.0);

	// params.minWeight = std::max<double>(optionMap.get(L"minWeight"sv).asFloat(0), std::numeric_limits<float>::epsilon());
	params.minWeight = 0.0; // doesn't work as expected

	return true;
}

SoundHandler::LinkingResult WeightedBlur::vFinishLinking(Logger& cl) {

	resamplerPtr = getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"BandResampler is not found in the source chain");
		return { };
	}

	const auto source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return { };
	}

	const auto dataSize = source->getDataSize();
	return dataSize;
}

void WeightedBlur::vProcess(const DataSupplier& dataSupplier) {
	changed = true;
}

void WeightedBlur::vFinish() {
	if (!changed) {
		return;
	}

	changed = false;

	auto& source = *getSource();
	source.finish();
	const BandResampler& resampler = *resamplerPtr;
	const index layersCount = source.getDataSize().layersCount;
	const auto sourceData = source.getData();

	double minRadius = params.minRadius * std::pow(params.minRadiusAdaptation, resampler.getStartingLayer());
	double maxRadius = params.maxRadius * std::pow(params.maxRadiusAdaptation, resampler.getStartingLayer());

	const auto refIds = getRefIds();

	for (index i = 0; i < layersCount; ++i) {
		const auto sid = sourceData.ids[i];

		if (sid != refIds[i]) {
			const auto cascadeMagnitudes = sourceData.values[i];
			const auto cascadeWeights = resampler.getBandWeights(i);

			const auto result = updateLayerData(i, sid);
			blurCascade(cascadeMagnitudes, cascadeWeights, result, minRadius, maxRadius);
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
		radius = std::min<index>(std::lround(minRadius), source.size());

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
