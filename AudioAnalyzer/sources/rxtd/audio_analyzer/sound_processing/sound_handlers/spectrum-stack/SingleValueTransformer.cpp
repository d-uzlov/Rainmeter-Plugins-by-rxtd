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

	return dataSize;
}

void SingleValueTransformer::vProcessLayer(array_view<float> chunk, array_span<float> dest, ExternalData& handlerSpecificData) {
	params.transformer.applyToArray(chunk, dest);
}
