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

using namespace audio_analyzer;

SoundHandler::ParseResult legacy_LogarithmicValueMapper::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain, index legacyNumber
) const {
	Params params;

	params.sourceId = om.get(L"source").asIString();
	if (params.sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	params.sensitivity = om.get(L"sensitivity").asFloat(35.0);
	params.sensitivity = std::clamp<double>(params.sensitivity, std::numeric_limits<float>::epsilon(), 1000.0);
	params.offset = om.get(L"offset").asFloatF(0.0);

	return params;
}

SoundHandler::LinkingResult legacy_LogarithmicValueMapper::vFinishLinking(Logger& cl) {
	logNormalization = float(20.0 / params.sensitivity);

	auto& config = getConfiguration();
	const auto dataSize = config.sourcePtr->getDataSize();
	return dataSize;
}

void legacy_LogarithmicValueMapper::vProcess(array_view<float> wave) {
	changed = true;
}

void legacy_LogarithmicValueMapper::vFinish() {
	if (!changed) {
		return;
	}

	changed = false;

	constexpr float log10inverse = 0.30102999566398119521; // 1.0 / log2(10)

	auto& config = getConfiguration();
	auto& source = *config.sourcePtr;
	source.finish();
	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; ++i) {
		for (auto chunk : source.getChunks(i)) {
			auto dest = generateLayerData(i, chunk.equivalentWaveSize);

			for (index j = 0; j < chunk.data.size(); ++j) {
				float value = utils::MyMath::fastLog2(chunk.data[j]) * log10inverse;
				value = value * logNormalization + 1.0f + params.offset;
				dest[j] = float(value);
			}
		}
	}
}
