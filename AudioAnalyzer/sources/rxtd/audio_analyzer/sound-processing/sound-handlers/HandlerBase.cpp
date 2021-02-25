/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HandlerBase.h"

#include "rxtd/StringUtils.h"

using rxtd::audio_analyzer::handler::HandlerBase;

bool HandlerBase::patch(
	sview name,
	const ParamsContainer& params, array_view<istring> sources,
	index sampleRate, Version version,
	HandlerFinder& hf, Logger& cl,
	Snapshot& snapshot
) {
	if (sources.size() > 1) {
		throw std::exception{ "no support for multiple sources yet" };
	}

	_name = name;

	Configuration newConfig;
	if (!sources.empty()) {
		const auto& sourceName = sources[0];
		newConfig.sourcePtr = hf.getHandler(sourceName);
		if (newConfig.sourcePtr == nullptr) {
			cl.error(L"source {} is not found", sourceName);
			return {};
		}

		const auto dataSize = newConfig.sourcePtr->getDataSize();
		if (dataSize.isEmpty()) {
			cl.error(L"source {} doesn't produce any data", sourceName);
			return {};
		}
	} else {
		newConfig.sourcePtr = nullptr;
	}
	newConfig.sampleRate = sampleRate;
	newConfig.version = version;

	if (_configuration != newConfig
		|| newConfig.sourcePtr != nullptr && newConfig.sourcePtr->_anyChanges
		|| !vCheckSameParams(params)) {
		_anyChanges = true;

		_configuration = newConfig;

		const auto linkingResult = vConfigure(params, cl, snapshot.handlerSpecificData);
		if (!linkingResult.success) {
			return {};
		}

		_dataSize = linkingResult.dataSize;
		_layers.clear();
		_layers.resize(linkingResult.dataSize.layersCount);
		_lastResults.setBuffersCount(linkingResult.dataSize.layersCount);
		_lastResults.setBufferSize(linkingResult.dataSize.valuesCount);
		_lastResults.fill(0.0f);

		auto& sv = snapshot.values;
		if (sv.getBuffersCount() != _dataSize.layersCount || sv.getBufferSize() != _dataSize.valuesCount) {
			snapshot.values.setBuffersCount(_dataSize.layersCount);
			snapshot.values.setBufferSize(_dataSize.valuesCount);
			snapshot.values.fill(0.0f);
		}
	}

	return true;
}

rxtd::index HandlerBase::legacy_parseIndexProp(
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
