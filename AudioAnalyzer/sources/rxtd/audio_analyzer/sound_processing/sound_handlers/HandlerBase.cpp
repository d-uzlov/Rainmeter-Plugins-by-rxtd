// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "HandlerBase.h"

#include "rxtd/std_fixes/StringUtils.h"

using rxtd::audio_analyzer::handler::HandlerBase;
using rxtd::std_fixes::StringUtils;

bool HandlerBase::patch(
	sview name,
	const ParamsContainer& params, HandlerBase* source,
	index sampleRate, Version version,
	Logger& cl,
	Snapshot& snapshot
) {
	_data.name = name;

	Configuration newConfig;
	newConfig.sourcePtr = source;
	if (source != nullptr) {
		const auto dataSize = newConfig.sourcePtr->getDataSize();
		if (dataSize.isEmpty()) {
			cl.error(L"source (handler {}) doesn't produce any data", source->_data.name);
			return {};
		}
	}
	newConfig.sampleRate = sampleRate;
	newConfig.version = version;

	auto sourceDataSize = newConfig.sourcePtr == nullptr ? DataSize{} : newConfig.sourcePtr->getDataSize();
	if (_configuration != newConfig
		|| sourceDataSize != _inputDataSize
		|| !vCheckSameParams(params)) {
		_inputDataSize = std::move(sourceDataSize);
		_configuration = newConfig;

		try {
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
		} catch(InvalidOptionsException&) {
			// todo refactor to propagate exception
			return {};
		}

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
