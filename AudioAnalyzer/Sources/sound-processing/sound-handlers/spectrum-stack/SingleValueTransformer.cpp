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

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool SingleValueTransformer::parseParams(
	const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr
) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), transformLogger);

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
		processStateful(wave.size());
	} else {
		processStateless();
	}
}

void SingleValueTransformer::processStateless() {
	auto& source = *getSource();
	source.finish();
	const auto sourceData = source.getData();
	const index layersCount = source.getDataSize().layersCount;

	const auto refIds = getRefIds();

	for (index i = 0; i < layersCount; ++i) {
		const auto sid = sourceData.ids[i];

		if (sid == refIds[i]) {
			continue;
		}

		auto layerData = sourceData.values[i];
		auto dest = updateLayerData(i, sid);

		params.transformer.applyToArray(layerData, dest);
	}
}

void SingleValueTransformer::processStateful(index waveSize) {
	auto& source = *getSource();
	source.finish();
	const auto sourceData = source.getData();
	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; ++i) {
		auto layerData = sourceData.values[i];
		auto dest = generateLayerData(i);

		transformers[i].setParams(getSampleRate(), waveSize);
		transformers[i].applyToArray(layerData, dest);
	}
}
