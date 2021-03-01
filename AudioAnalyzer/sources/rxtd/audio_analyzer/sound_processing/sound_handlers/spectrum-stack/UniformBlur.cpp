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

using rxtd::audio_analyzer::handler::UniformBlur;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer UniformBlur::vParseParams(ParamParseContext& context) const {
	ParamsContainer result;
	auto& params = result.clear<Params>();

	const auto sourceId = context.options.get(L"source").asIString();
	if (sourceId.empty()) {
		context.log.error(L"source is not found");
		throw InvalidOptionsException{};
	}

	//                                                        ?? ↓↓ looks best ?? at 0.25 ↓↓ ??
	params.blurRadius = std::max<double>(context.options.get(L"Radius").asFloat(1.0) * 0.25, 0.0);
	params.blurRadiusAdaptation = std::max<double>(context.options.get(L"RadiusAdaptation").asFloat(2.0), 0.0);

	return result;
}

HandlerBase::ConfigurationResult
UniformBlur::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();

	const auto dataSize = config.sourcePtr->getDataSize();
	return dataSize;
}

void UniformBlur::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;

	double theoreticalRadius = params.blurRadius;

	const index cascadesCount = source.getDataSize().layersCount;
	for (index i = 0; i < cascadesCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			auto cascadeResult = pushLayer(i);

			const index radius = std::llround(theoreticalRadius);
			if (radius < 1 || clock::now() > context.killTime) {
				chunk.transferToSpan(cascadeResult);
			} else {
				blurCascade(chunk, cascadeResult, radius);
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
			if (bandIndex >= bandsCount || kernelIndex >= static_cast<index>(kernel.size())) {
				break;
			}
			result += kernel[kernelIndex] * source[bandIndex];

			kernelIndex++;
			bandIndex++;
		}

		dest[i] = static_cast<float>(result);
	}
}
