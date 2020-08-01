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

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool legacy_LogarithmicValueMapper::parseParams(
	const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr
) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.sourceId = optionMap.get(L"source"sv).asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return {};
	}

	params.sensitivity = optionMap.get(L"sensitivity"sv).asFloat(35.0);
	params.sensitivity = std::clamp<double>(params.sensitivity, std::numeric_limits<float>::epsilon(), 1000.0);
	params.offset = optionMap.get(L"offset"sv).asFloatF(0.0);

	return true;
}

void legacy_LogarithmicValueMapper::setParams(const Params& value) {
	params = value;

	logNormalization = float(20.0 / params.sensitivity);
}

bool legacy_LogarithmicValueMapper::vFinishLinking(Logger& cl) {
	sourcePtr = getSource();
	if (sourcePtr == nullptr) {
		cl.error(L"source is not found");
		return false;
	}

	const auto [layersCount, valuesCount] = sourcePtr->getDataSize();
	values.setBuffersCount(layersCount);
	values.setBufferSize(valuesCount);

	layers.resize(layersCount);
	for (index i = 0; i < layersCount; ++i) {
		layers[i].values = values[i];
	}

	return true;
}

void legacy_LogarithmicValueMapper::vProcess(const DataSupplier& dataSupplier) {
	changed = true;
}

void legacy_LogarithmicValueMapper::vFinish() {
	if (!changed) {
		return;
	}

	changed = false;

	constexpr float log10inverse = 0.30102999566398119521; // 1.0 / log2(10)

	auto& source = *sourcePtr;
	source.finish();
	const auto [layersCount, valuesCount] = sourcePtr->getDataSize();

	const auto sourceData = source.vGetData();

	for (index layer = 0; layer < layersCount; ++layer) {
		// const auto values = sourceData[layer].values;
		auto sd = sourceData[layer];

		if (sd.id == layers[layer].id) {
			continue;
		}

		layers[layer].id = sd.id;

		auto sourceValues = sd.values;
		auto resultValues = values[layer];

		for (index i = 0; i < valuesCount; ++i) {
			float value = utils::MyMath::fastLog2(sourceValues[i]) * log10inverse;
			value = value * logNormalization + 1.0f + params.offset;
			resultValues[i] = float(value);
		}
	}
}

void legacy_LogarithmicValueMapper::vReset() {
	changed = true;
}
