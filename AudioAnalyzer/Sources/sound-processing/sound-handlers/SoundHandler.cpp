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
	const std::any& params, const std::vector<istring>& sources,
	sview channelName, index sampleRate,
	HandlerFinder& hf, Logger& cl
) {
	if (sources.size() > 1) {
		throw std::exception{ "no support for multiple sources yet" }; // todo
	}

	if (!checkSameParams(params)) {
		// todo add check that source changed and it matters
		setParams(params);
	}

	_configuration.sourcePtr = nullptr;
	if (!sources.empty()) {
		const auto& sourceName = sources[0];
		_configuration.sourcePtr = hf.getHandler(sourceName);
		if (_configuration.sourcePtr == nullptr) {
			cl.error(L"source '{}' is not found", sourceName);
			return { };
		}

		const auto dataSize = _configuration.sourcePtr->getDataSize();
		if (dataSize.isEmpty()) {
			cl.error(L"source '{}' doesn't produce any data", sourceName);
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
	_lastResults.fill(0.0f);

	return true;
}

void SoundHandler::configureSnapshot(Snapshot& snapshot) const {
	snapshot.values.setBuffersCount(_dataSize.layersCount);
	snapshot.values.setBufferSize(_dataSize.valuesCount);
	snapshot.values.fill(0.0f);
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
