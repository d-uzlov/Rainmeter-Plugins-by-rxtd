/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SingleValueTransformer.h"

using rxtd::audio_analyzer::handler::SingleValueTransformer;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer SingleValueTransformer::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	const auto sourceId = context.options.get(L"source").asIString();
	if (sourceId.empty()) {
		context.log.error(L"source is not found");
		throw InvalidOptionsException{};
	}

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(context.options.get(L"transform").asString(), transformLogger);

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
