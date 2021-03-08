// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "SingleValueTransformer.h"

using rxtd::audio_analyzer::handler::SingleValueTransformer;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer SingleValueTransformer::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(context.options.get(L"transform").asString(), context.parser, transformLogger);

	return params;
}

HandlerBase::ConfigurationResult
SingleValueTransformer::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const auto dataSize = config.sourcePtr->getDataSize();

	transformersPerLayer.resize(static_cast<size_t>(dataSize.layersCount));
	for (auto& tr : transformersPerLayer) {
		tr = params.transformer;
	}

	return dataSize;
}

void SingleValueTransformer::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;
	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			auto dest = pushLayer(i);

			params.transformer.applyToArray(chunk, dest);
		}
	}
}
