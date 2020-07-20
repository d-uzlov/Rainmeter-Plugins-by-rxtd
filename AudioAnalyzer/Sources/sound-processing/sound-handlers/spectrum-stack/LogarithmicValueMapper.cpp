/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "LogarithmicValueMapper.h"
#include "FastMath.h"
#include "option-parser/OptionMap.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<LogarithmicValueMapper::Params> LogarithmicValueMapper::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl
) {
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

void LogarithmicValueMapper::setParams(Params _params, Channel channel) {
	this->params = std::move(_params);

	logNormalization = 20.0 / params.sensitivity;

	valid = true;
}

void LogarithmicValueMapper::process(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	changed = true;
}

void LogarithmicValueMapper::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

void LogarithmicValueMapper::finish(const DataSupplier& dataSupplier) {
	if (changed) {
		updateValues(dataSupplier);
		changed = false;
	}
}

void LogarithmicValueMapper::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
}

const wchar_t* LogarithmicValueMapper::getProp(const isview& prop) const {
	propString.clear();

	return nullptr;
}

void LogarithmicValueMapper::reset() {
	changed = true;
}


void LogarithmicValueMapper::updateValues(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	valid = false;

	const auto source = dataSupplier.getHandler(params.sourceId);
	if (source == nullptr) {
		return;
	}

	transformToLog(*source);

	valid = true;
}

void LogarithmicValueMapper::transformToLog(const SoundHandler& source) {
	constexpr double log10inverse = 0.30102999566398119521; // 1.0 / log2(10)

	const auto layersCount = source.getLayersCount();
	resultValues.resize(layersCount); // TODO use my vector2D ?

	for (layer_t layer = 0; layer < layersCount; ++layer) {
		const auto values = source.getData(layer);

		resultValues[layer].resize(values.size()); // TODO use my vector2D ?

		for (index i = 0; i < values.size(); ++i) {
			double value = values[i];

			value = utils::FastMath::log2(float(value)) * log10inverse;
			value = value * logNormalization + 1.0;
			value += params.offset;
			resultValues[layer][i] = value;
		}
	}

}
