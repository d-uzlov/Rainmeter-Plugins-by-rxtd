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

bool SoundHandler::patch(
	const std::any& params,
	sview channelName, index sampleRate,
	HandlerFinder& hf, Logger& cl
) {
	if (!checkSameParams(params)) {
		setParams(params);
	}

	if (const auto sourceName = vGetSourceName();
		!sourceName.empty()) {
		_configuration.sourcePtr = hf.getHandler(sourceName);
		if (_configuration.sourcePtr == nullptr) {
			cl.error(L"source is not found");
			return { };
		}

		const auto dataSize = _configuration.sourcePtr->getDataSize();
		if (dataSize.isEmpty()) {
			cl.error(L"source doesn't produce any data");
			return { };
		}
	}

	_configuration.sampleRate = sampleRate;
	_configuration.channelName = channelName;

	const auto linkingResult = vConfigure(cl);
	if (!linkingResult.success) {
		return { };
	}

	_dataSize = linkingResult.dataSize;
	_layers.clear();
	_layers.resize(linkingResult.dataSize.layersCount);
	_lastResults.setBuffersCount(linkingResult.dataSize.layersCount);
	_lastResults.setBufferSize(linkingResult.dataSize.valuesCount);
	_lastResults.init(0.0f);

	return true;
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
