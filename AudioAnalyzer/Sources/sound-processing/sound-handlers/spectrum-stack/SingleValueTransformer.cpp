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

std::optional<SingleValueTransformer::Params> SingleValueTransformer::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl
) {
	Params params;

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), cl);

	return params;
}

void SingleValueTransformer::setParams(Params _params, Channel channel) {
	if (this->params == _params) {
		return;
	}

	params = std::move(_params);
	params.transformer.setParams(samplesPerSec, 1);

	setResamplerID(params.sourceId);
}

void SingleValueTransformer::setSamplesPerSec(index value) {
	samplesPerSec = value;
}

void SingleValueTransformer::reset() {
	params.transformer.resetState();
}

void SingleValueTransformer::_process(const DataSupplier& dataSupplier) {
	const auto source = dataSupplier.getHandler(params.sourceId);
	if (source == nullptr) {
		setValid(false);
		return;
	}

	const index layersCount = source->getLayersCount();
	const index layerSize = source->getData(0).size();

	values.setBuffersCount(layersCount);
	values.setBufferSize(layerSize);

	for (index layerIndex = 0; layerIndex < layersCount; ++layerIndex) {
		auto layerData = source->getData(layerIndex);
		auto dest = values[layerIndex];

		std::copy(layerData.begin(), layerData.end(), dest.begin());
	}

	params.transformer.setParams(samplesPerSec, dataSupplier.getWave().size());
	params.transformer.applyToArray(values);
}
