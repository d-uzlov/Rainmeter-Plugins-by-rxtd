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
		return { };
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

SoundHandler::LinkingResult legacy_LogarithmicValueMapper::vFinishLinking(Logger& cl) {
	const auto sourcePtr = getSource();
	if (sourcePtr == nullptr) {
		cl.error(L"source is not found");
		return { };
	}

	const auto dataSize = sourcePtr->getDataSize();

	return dataSize;
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

	auto& source = *getSource();
	source.finish();
	const index layersCount = source.getDataSize().layersCount;

	const auto sourceData = source.getData();

	const auto refIds = getRefIds();

	for (index i = 0; i < layersCount; ++i) {
		const auto sid = sourceData.ids[i];

		if (sid == refIds[i]) {
			continue;
		}

		auto sd = sourceData.values[i];
		auto dest = updateLayerData(i, sid);

		for (index j = 0; j < sd.size(); ++j) {
			float value = utils::MyMath::fastLog2(sd[j]) * log10inverse;
			value = value * logNormalization + 1.0f + params.offset;
			dest[j] = float(value);
		}
	}
}
