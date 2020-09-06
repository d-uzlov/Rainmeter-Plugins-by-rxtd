/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "TimeResampler.h"

using namespace audio_analyzer;

SoundHandler::ParseResult TimeResampler::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	ParseResult result{ true };
	auto& params = result.params.clear<Params>();

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source is not found");
		return { };
	}

	params.granularity = om.get(L"granularity").asFloat(1000.0 / 60.0);
	params.granularity = std::max(params.granularity, 0.01);
	params.granularity *= 0.001;

	params.attack = om.get(L"attack").asFloat(0.0);
	params.decay = om.get(L"decay").asFloat(params.attack);

	params.attack = std::max(params.attack, 0.0);
	params.decay = std::max(params.decay, 0.0);

	params.attack = params.attack * 0.001;
	params.decay = params.decay * 0.001;

	result.sources.emplace_back(sourceId);
	return result;
}

SoundHandler::ConfigurationResult
TimeResampler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	auto& dataSize = config.sourcePtr->getDataSize();

	if (index(layersData.size()) != dataSize.layersCount) {
		layersData.resize(dataSize.layersCount);
		for (auto& ld : layersData) {
			ld.values.resize(dataSize.valuesCount);
			std::fill(ld.values.begin(), ld.values.end(), 0.0f);
			ld.dataCounter = 0;
			ld.waveCounter = 0;
		}
	} else if (index(layersData.front().values.size()) != dataSize.valuesCount) {
		for (auto& ld : layersData) {
			ld.values.resize(dataSize.valuesCount);
			std::fill(ld.values.begin(), ld.values.end(), 0.0f);
			// no need for resetting counters here
		}
	}

	blockSize = index(params.granularity * config.sampleRate);

	for (index i = 0; i < dataSize.layersCount; ++i) {
		layersData[i].lowPass.setParams(
			params.attack, params.decay,
			getConfiguration().sampleRate,
			std::min(dataSize.eqWaveSizes[i], blockSize)
		);
	}

	std::vector<index> eqWS;
	eqWS.resize(dataSize.layersCount);
	for (int i = 0; i < dataSize.layersCount; ++i) {
		eqWS.push_back(blockSize);
	}
	return dataSize;
}

void TimeResampler::vProcess(ProcessContext context, ExternalData& externalData) {
	const auto& source = *getConfiguration().sourcePtr;
	const index layersCount = source.getDataSize().layersCount;
	for (int i = 0; i < layersCount; ++i) {
		processLayer(context.wave.size(), i);
	}
}

void TimeResampler::processLayer(index waveSize, index layer) {
	LayerData& ld = layersData[layer];
	const auto& source = *getConfiguration().sourcePtr;
	ld.waveCounter += waveSize;

	auto lastValue = source.getSavedData(layer);

	const index equivalentWaveSize = source.getDataSize().eqWaveSizes[layer];

	for (auto chunk : source.getChunks(layer)) {
		ld.dataCounter += equivalentWaveSize;
		lastValue = chunk;

		if (ld.dataCounter < blockSize) {
			ld.lowPass.arrayApply(ld.values, chunk);
			continue;
		}

		while (ld.dataCounter >= blockSize && ld.waveCounter >= blockSize) {
			ld.lowPass.arrayApply(ld.values, chunk);
			pushLayer(layer).copyFrom(ld.values);

			ld.dataCounter -= blockSize;
			ld.waveCounter -= blockSize;
		}
	}

	// this ensures that push speed is consistent regardless of input latency
	while (ld.waveCounter >= blockSize) {
		ld.lowPass.arrayApply(ld.values, lastValue);
		pushLayer(layer).copyFrom(ld.values);

		ld.waveCounter -= blockSize;
		if (ld.dataCounter >= blockSize) {
			ld.dataCounter -= blockSize;
		}
	}
}
