/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "HandlerBase.h"

#include "rxtd/std_fixes/StringUtils.h"

using rxtd::audio_analyzer::handler::HandlerBase;
using rxtd::std_fixes::StringUtils;

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

	_data.name = name;

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

	auto sourceDataSize = newConfig.sourcePtr == nullptr ? DataSize{} : newConfig.sourcePtr->getDataSize();
	if (_configuration != newConfig
		|| sourceDataSize != _inputDataSize
		|| !vCheckSameParams(params)) {
		_inputDataSize = std::move(sourceDataSize);
		_configuration = newConfig;

		const auto linkingResult = vConfigure(params, cl, snapshot.handlerSpecificData);
		if (!linkingResult.success) {
			return {};
		}

		_data.size = linkingResult.dataSize;
		_data.layers.inflatableCache.clear();
		_data.layers.inflatableCache.resize(static_cast<size_t>(linkingResult.dataSize.layersCount));
		_data.lastResults.setBuffersCount(linkingResult.dataSize.layersCount);
		_data.lastResults.setBufferSize(linkingResult.dataSize.valuesCount);
		_data.lastResults.fill(0.0f);

		auto& sv = snapshot.values;
		if (sv.getBuffersCount() != _data.size.layersCount || sv.getBufferSize() != _data.size.valuesCount) {
			snapshot.values.setBuffersCount(_data.size.layersCount);
			snapshot.values.setBufferSize(_data.size.valuesCount);
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

	const auto cascadeIndex = StringUtils::parseInt(indexView % csView());

	if (cascadeIndex < minBound || cascadeIndex >= endBound) {
		return -2;
	}

	return cascadeIndex;
}
