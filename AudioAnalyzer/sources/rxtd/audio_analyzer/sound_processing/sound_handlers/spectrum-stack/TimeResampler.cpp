// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "TimeResampler.h"

using rxtd::audio_analyzer::handler::TimeResampler;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer TimeResampler::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	params.granularity = context.parser.parse(context.options, L"granularity").valueOr(1000.0 / 60.0);
	params.granularity = std::max(params.granularity, 0.01);
	params.granularity *= 0.001;

	params.attack = context.parser.parse(context.options, L"attack").valueOr(0.0);
	params.decay = context.parser.parse(context.options, L"decay").valueOr(params.attack);

	params.attack = std::max(params.attack, 0.0);
	params.decay = std::max(params.decay, 0.0);

	params.attack = params.attack * 0.001;
	params.decay = params.decay * 0.001;

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(context.options.get(L"transform").asString(), context.parser, transformLogger);

	return params;
}

HandlerBase::ConfigurationResult
TimeResampler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	auto& dataSize = config.sourcePtr->getDataSize();

	if (static_cast<index>(layersData.size()) != dataSize.layersCount) {
		layersData.resize(static_cast<size_t>(dataSize.layersCount));
		for (auto& ld : layersData) {
			ld.values.resize(static_cast<size_t>(dataSize.valuesCount));
			std::fill(ld.values.begin(), ld.values.end(), 0.0f);
			ld.dataCounter = 0;
			ld.waveCounter = 0;
		}
	} else if (static_cast<index>(layersData.front().values.size()) != dataSize.valuesCount) {
		for (auto& ld : layersData) {
			ld.values.resize(static_cast<size_t>(dataSize.valuesCount));
			std::fill(ld.values.begin(), ld.values.end(), 0.0f);
			// no need for resetting counters here
		}
	}

	blockSize = static_cast<index>(params.granularity * static_cast<double>(config.sampleRate));
	blockSize = std::max<index>(blockSize, 1);

	for (index i = 0; i < dataSize.layersCount; ++i) {
		layersData[static_cast<size_t>(i)].lowPass.setParams(
			params.attack, params.decay,
			getConfiguration().sampleRate,
			std::min(dataSize.eqWaveSizes[static_cast<size_t>(i)], blockSize)
		);
	}

	std::vector<index> eqWs(static_cast<size_t>(dataSize.layersCount), blockSize);
	return { dataSize.valuesCount, std::move(eqWs) };
}

void TimeResampler::vProcess(ProcessContext context, ExternalData& externalData) {
	const auto& source = *getConfiguration().sourcePtr;
	const index layersCount = source.getDataSize().layersCount;
	for (int i = 0; i < layersCount; ++i) {
		processLayer(context.wave.size(), i);
	}
}

void TimeResampler::processLayer(index waveSize, index layer) {
	LayerData& ld = layersData[static_cast<size_t>(layer)];
	const auto& source = *getConfiguration().sourcePtr;
	ld.waveCounter += waveSize;

	auto lastValue = source.getSavedData(layer);

	const index equivalentWaveSize = source.getDataSize().eqWaveSizes[static_cast<size_t>(layer)];

	for (auto chunk : source.getChunks(layer)) {
		ld.dataCounter += equivalentWaveSize;
		lastValue = chunk;

		if (ld.dataCounter < blockSize) {
			ld.lowPass.arrayApply(chunk, ld.values);
			continue;
		}

		while (ld.dataCounter >= blockSize && ld.waveCounter >= blockSize) {
			ld.lowPass.arrayApply(chunk, ld.values);
			auto result = pushLayer(layer);
			result.copyFrom(ld.values);
			params.transformer.applyToArray(result, result);

			ld.dataCounter -= blockSize;
			ld.waveCounter -= blockSize;
		}
	}

	// this ensures that push speed is consistent regardless of input latency
	while (ld.waveCounter >= blockSize) {
		ld.lowPass.arrayApply(lastValue, ld.values);
		auto result = pushLayer(layer);
		result.copyFrom(ld.values);
		params.transformer.applyToArray(result, result);

		ld.waveCounter -= blockSize;
		if (ld.dataCounter >= blockSize) {
			ld.dataCounter -= blockSize;
		}
	}
}
