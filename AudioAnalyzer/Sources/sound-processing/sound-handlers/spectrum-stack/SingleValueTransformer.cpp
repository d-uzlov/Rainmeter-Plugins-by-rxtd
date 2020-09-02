/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SingleValueTransformer.h"

using namespace audio_analyzer;

SoundHandler::ParseResult SingleValueTransformer::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return { };
	}

	params.granularity = om.get(L"granularity").asFloat(0.0);
	params.granularity *= 0.001;

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(om.get(L"transform").asString(), transformLogger);

	ParseResult result{ true };
	result.params = std::move(params);
	result.sources.emplace_back(sourceId);
	return result;
}

SoundHandler::ConfigurationResult
SingleValueTransformer::vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) {
	params = std::any_cast<Params>(_params);

	auto& config = getConfiguration();
	const auto dataSize = config.sourcePtr->getDataSize();

	params.transformer.setHistoryWidth(dataSize.valuesCount);

	transformersPerLayer.resize(dataSize.layersCount);
	for (auto& tr : transformersPerLayer) {
		tr = params.transformer;
	}

	granularityBlock = index(params.granularity * config.sampleRate);
	for (auto& transformer : transformersPerLayer) {
		transformer.setParams(config.sampleRate, granularityBlock);
	}

	return dataSize;
}

void SingleValueTransformer::vProcess(ProcessContext context) {
	if (params.transformer.hasState()) {
		processStateful();
	} else {
		processStateless();
	}
}

void SingleValueTransformer::processStateless() {
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

void SingleValueTransformer::processStateful() {
	// todo remove function
}
