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

std::atomic<LayerDataId::idType> LayerDataId::sourceIdCounter{ 1 };

SoundHandler* SoundHandler::patch(
	SoundHandler* old, HandlerPatcher& patcher,
	Channel channel, index sampleRate,
	HandlerFinder& hf, Logger& cl
) {
	auto& result = *patcher.patch(old);

	const auto sourceName = result.vGetSourceName();
	result._sourceHandlerPtr = hf.getHandler(sourceName);

	if (!sourceName.empty()) {
		if (result._sourceHandlerPtr == nullptr) {
			cl.error(L"source is not found");
			result.setValid(false);
			return &result;
		}

		const auto dataSize = result._sourceHandlerPtr->getDataSize();
		if (dataSize.isEmpty()) {
			cl.error(L"source doesn't produce any data");
			result.setValid(false);
			return &result;
		}
	}

	result._sampleRate = sampleRate;
	result._channel = channel;

	const auto linkingResult = result.vFinishLinking(cl);
	result.setValid(linkingResult.success);

	if (linkingResult.success) {
		result._dataSize = linkingResult.dataSize;
		result._values.setBuffersCount(linkingResult.dataSize.layersCount);
		result._values.setBufferSize(linkingResult.dataSize.valuesCount);

		const index oldSize = result._ids.size();
		if (linkingResult.dataSize.layersCount > oldSize) {
			result._ids.reserve(linkingResult.dataSize.layersCount);

			for (index i = 0; i < linkingResult.dataSize.layersCount - oldSize; i++) {
				result._ids.push_back(result._generatorId);
			}
		}

		result._idsRef.clear();
		result._idsRef.resize(linkingResult.dataSize.layersCount);
	} else {
		result._dataSize = { };
		result._values.setBuffersCount(0);
		result._values.setBufferSize(0);
		result._ids = { };
	}

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
