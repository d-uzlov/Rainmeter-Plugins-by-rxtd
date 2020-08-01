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
		return {};
	}

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), transformLogger);

	return true;
}

void SingleValueTransformer::setParams(const Params& value) {
	params = value;
}

bool SingleValueTransformer::vFinishLinking(Logger& cl) {
	const auto source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	// todo check for zero size
	const auto [layersCount, layerSize] = source->getDataSize();

	values.setBuffersCount(layersCount);
	values.setBufferSize(layerSize);
	layers.resize(layersCount);

	for (index layerIndex = 0; layerIndex < layersCount; ++layerIndex) {
		// layers[layerIndex].id = 0;
		layers[layerIndex].values = values[layerIndex];
	}

	return true;
}

void SingleValueTransformer::vReset() {
	params.transformer.resetState();
}

void SingleValueTransformer::vProcess(const DataSupplier& dataSupplier) {
	auto& source = *getSource();

	source.finish();
	const auto sourceData = source.vGetData();
	const auto [layersCount, layerSize] = source.getDataSize();

	for (index layerIndex = 0; layerIndex < layersCount; ++layerIndex) {
		auto layerData = sourceData[layerIndex];
		auto dest = values[layerIndex];

		// todo check if transform is stateless and then check .id
		std::copy(layerData.values.begin(), layerData.values.end(), dest.begin());

		layers[layerIndex].id++;
	}

	params.transformer.setParams(getSampleRate(), dataSupplier.getWave().size());
	params.transformer.applyToArray(values);
}
