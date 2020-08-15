/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SingleValueTransformer.h"
#include "option-parser/OptionMap.h"

using namespace audio_analyzer;

bool SingleValueTransformer::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain, void* paramsPtr, index legacyNumber
) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = om.get(L"source").asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(om.get(L"transform").asString(), transformLogger);

	return true;
}

void SingleValueTransformer::setParams(const Params& value) {
	params = value;
}

SoundHandler::LinkingResult SingleValueTransformer::vFinishLinking(Logger& cl) {
	const auto dataSize = getSource()->getDataSize();

	params.transformer.setHistoryWidth(dataSize.valuesCount);

	transformers.resize(dataSize.layersCount);
	for (auto& tr : transformers) {
		tr = params.transformer;
	}

	return dataSize;
}

void SingleValueTransformer::vReset() {
	params.transformer.resetState();
}

void SingleValueTransformer::vProcess(array_view<float> wave) {
	if (params.transformer.hasState()) {
		processStateful();
	} else {
		processStateless();
	}
}

void SingleValueTransformer::processStateless() {
	auto& source = *getSource();
	source.finish();
	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			auto layerData = chunk.data;
			auto dest = generateLayerData(i, chunk.size);

			params.transformer.applyToArray(layerData, dest);
		}
	}
}

void SingleValueTransformer::processStateful() {
	auto& source = *getSource();
	source.finish();
	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			auto layerData = chunk.data;
			auto dest = generateLayerData(i, chunk.size);

			// todo resample in time
			transformers[i].setParams(getSampleRate(), chunk.size);
			params.transformer.applyToArray(layerData, dest);
		}
	}
}
