// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "UniformBlur.h"
#include "rxtd/audio_analyzer/audio_utils/GaussianCoefficientsManager.h"

using rxtd::audio_analyzer::handler::UniformBlur;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer UniformBlur::vParseParams(ParamParseContext& context) const {
	Params params;

	params.blurRadius = context.parser.parse(context.options, L"Radius").valueOr(1.0f);
	if (params.blurRadius < 0.0f) {
		context.log.error(L"radius: value must be >= 0.0");
		throw InvalidOptionsException{};
	}

	params.blurCoefficients = audio_utils::GaussianCoefficientsManager::createGaussianKernel(params.blurRadius);
	if (params.blurCoefficients.size() < 3) {
		context.log.error(L"radius: value is too small, remove this handler is you don't need blur");
		throw InvalidOptionsException{};
	}

	return params;
}

HandlerBase::ConfigurationResult
UniformBlur::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();

	const auto dataSize = config.sourcePtr->getDataSize();
	return dataSize;
}

void UniformBlur::vProcessLayer(array_view<float> chunk, array_span<float> dest, ExternalData& handlerSpecificData) {
	auto kernel = array_view<float>{ params.blurCoefficients };

	auto radius = kernel.size() / 2;

	const auto bandsCount = chunk.size();
	for (index i = 0; i < bandsCount; ++i) {

		index bandStartIndex = i - radius;
		index kernelStartIndex = 0;

		if (bandStartIndex < 0) {
			kernelStartIndex = -bandStartIndex;
			bandStartIndex = 0;
		}

		float result = 0.0f;
		index kernelIndex = kernelStartIndex;
		index bandIndex = bandStartIndex;

		while (bandIndex < bandsCount && kernelIndex < kernel.size()) {
			result += kernel[kernelIndex] * chunk[bandIndex];

			kernelIndex++;
			bandIndex++;
		}

		dest[i] = result;
	}
}
