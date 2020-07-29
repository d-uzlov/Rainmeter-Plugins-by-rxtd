/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "legacy_LogarithmicValueMapper.h"
#include "MyMath.h"
#include "option-parser/OptionMap.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<legacy_LogarithmicValueMapper::Params>
legacy_LogarithmicValueMapper::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params;
	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return std::nullopt;
	}

	params.sensitivity = std::clamp<double>(
		optionMap.get(L"sensitivity"sv).asFloat(35.0),
		std::numeric_limits<float>::epsilon(),
		1000.0
	);
	params.offset = optionMap.get(L"offset"sv).asFloat(0.0);

	return params;
}

void legacy_LogarithmicValueMapper::setParams(Params _params, Channel channel) {
	this->params = std::move(_params);

	logNormalization = 20.0 / params.sensitivity;

	setValid(true);
}

void legacy_LogarithmicValueMapper::_process(const DataSupplier& dataSupplier) {
	changed = true;
}

void legacy_LogarithmicValueMapper::_finish(const DataSupplier& dataSupplier) {
	if (changed) {
		updateValues(dataSupplier);
		changed = false;
	}
}

void legacy_LogarithmicValueMapper::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

void legacy_LogarithmicValueMapper::reset() {
	changed = true;
}


void legacy_LogarithmicValueMapper::updateValues(const DataSupplier& dataSupplier) {
	setValid(false);

	const auto source = dataSupplier.getHandler(params.sourceId);
	if (source == nullptr) {
		return;
	}

	transformToLog(*source);

	setValid(true);
}

void legacy_LogarithmicValueMapper::transformToLog(const SoundHandler& source) {
	constexpr double log10inverse = 0.30102999566398119521; // 1.0 / log2(10)

	const auto layersCount = source.getLayersCount();
	resultValues.resize(layersCount); // TODO use my vector2D ?

	for (index layer = 0; layer < layersCount; ++layer) {
		const auto values = source.getData(layer);

		resultValues[layer].resize(values.size()); // TODO use my vector2D ?

		for (index i = 0; i < values.size(); ++i) {
			double value = values[i];

			value = utils::MyMath::fastLog2(float(value)) * log10inverse;
			value = value * logNormalization + 1.0;
			value += params.offset;
			resultValues[layer][i] = float(value);
		}
	}

}
