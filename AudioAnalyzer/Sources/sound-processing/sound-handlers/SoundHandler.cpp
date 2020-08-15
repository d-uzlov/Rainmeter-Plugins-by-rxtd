/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SoundHandler.h"
#include "StringUtils.h"

using namespace audio_analyzer;

SoundHandler* SoundHandler::patch(
	SoundHandler* old, HandlerPatcher& patcher,
	Channel channel, index sampleRate,
	HandlerFinder& hf, Logger& cl
) {
	auto& result = *patcher.patch(old);

	if (const auto sourceName = result.vGetSourceName();
		!sourceName.empty()) {
		result._sourceHandlerPtr = hf.getHandler(sourceName);
		if (result._sourceHandlerPtr == nullptr) {
			cl.error(L"source is not found");
			return { };
		}

		const auto dataSize = result._sourceHandlerPtr->getDataSize();
		if (dataSize.isEmpty()) {
			cl.error(L"source doesn't produce any data");
			return { };
		}
	}

	result._sampleRate = sampleRate;
	result._channel = channel;

	const auto linkingResult = result.vFinishLinking(cl);
	if (!linkingResult.success) {
		return { };
	}

	result._dataSize = linkingResult.dataSize;
	result._layers.resize(linkingResult.dataSize.layersCount);
	result._lastResults.setBuffersCount(linkingResult.dataSize.layersCount);
	result._lastResults.setBufferSize(linkingResult.dataSize.valuesCount);
	result._lastResults.init(0.0f);

	return &result;
}

index SoundHandler::legacy_parseIndexProp(
	const isview& request, const isview& propName,
	index minBound, index endBound
) {
	const auto indexPos = request.find(propName);

	if (indexPos != 0) {
		return -1;
	}

	const auto indexView = request.substr(propName.length());
	if (indexView.length() == 0) {
		return 0;
	}

	const auto cascadeIndex = utils::StringUtils::parseInt(indexView % csView());

	if (cascadeIndex < minBound || cascadeIndex >= endBound) {
		return -2;
	}

	return cascadeIndex;
}
