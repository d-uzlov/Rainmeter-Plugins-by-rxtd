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

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<SingleValueTransformer::Params>
SingleValueTransformer::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params;

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), transformLogger);

	return params;
}

void SingleValueTransformer::setParams(const Params& _params, Channel channel) {
	if (params == _params) {
		return;
	}

	params = _params;
	params.transformer.setParams(samplesPerSec, 1);
}

void SingleValueTransformer::setSamplesPerSec(index value) {
	samplesPerSec = value;
}

void SingleValueTransformer::reset() {
	params.transformer.resetState();
}

void SingleValueTransformer::_process(const DataSupplier& dataSupplier) {
	const auto source = getSource();

	source->finish();

	const auto sourceData = source->getData();
	const auto layersCount = sourceData.size();
	const index layerSize = sourceData[0].values.size();

	values.setBuffersCount(layersCount);
	values.setBufferSize(layerSize);
	layers.resize(layersCount);

	for (index layerIndex = 0; layerIndex < layersCount; ++layerIndex) {
		auto layerData = sourceData[layerIndex];
		auto dest = values[layerIndex];

		std::copy(layerData.values.begin(), layerData.values.end(), dest.begin());

		layers[layerIndex].id++;
		layers[layerIndex].values = dest;
	}

	params.transformer.setParams(samplesPerSec, dataSupplier.getWave().size());
	params.transformer.applyToArray(values);
}

bool SingleValueTransformer::vCheckSources(Logger& cl) {
	const auto source = getSource();
	if (source == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	return true;
}
