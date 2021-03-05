// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "Loudness.h"

#include "rxtd/std_fixes/MyMath.h"

using rxtd::std_fixes::MyMath;
using rxtd::audio_analyzer::handler::Loudness;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer Loudness::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(context.options.get(L"transform").asString(), context.parser, transformLogger);

	params.gatingLimit = context.parser.parseFloat(context.options.get(L"gatingLimit"), 0.5);
	params.gatingLimit = std::clamp(params.gatingLimit, 0.0, 1.0);

	params.updatesPerSecond = context.parser.parseFloat(context.options.get(L"updateRate"), 20.0);
	params.updatesPerSecond = std::clamp(params.updatesPerSecond, 0.01, 60.0);

	params.timeWindowMs = context.parser.parseFloat(context.options.get(L"timeWindow"), 1000.0);
	params.timeWindowMs = std::clamp(params.timeWindowMs, 0.01, 10000.0);

	params.gatingDb = context.parser.parseFloat(context.options.get(L"gatingDb"), -20.0);
	params.gatingDb = std::clamp(params.gatingDb, -70.0, 0.0);

	params.ignoreGatingForSilence = context.parser.parseBool(context.options.get(L"ignoreGatingForSilence"), true);

	return params;
}

HandlerBase::ConfigurationResult Loudness::vConfigure(
	const ParamsContainer& _params, Logger& cl,
	ExternalData& externalData
) {
	params = _params.cast<Params>();

	blocksCount = static_cast<index>(params.timeWindowMs / 1000.0 * params.updatesPerSecond);
	blocksCount = std::max<index>(blocksCount, 1);

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;

	blockSize = static_cast<index>(static_cast<double>(sampleRate) / params.updatesPerSecond);
	blockNormalizer = 1.0 / static_cast<double>(blockSize);
	blockCounter = 0;

	blocks.resize(static_cast<size_t>(blocksCount));
	std::fill(blocks.begin(), blocks.end(), 0.0);
	prevValue = 0.0;

	gatingValueCoefficient = MyMath::db2amplitude(params.gatingDb) * static_cast<double>(blockSize);

	minBlocksCount = static_cast<index>(static_cast<double>(blocksCount) * (1.0 - params.gatingLimit));

	return { 1, { blockSize } };
}

void Loudness::vProcess(ProcessContext context, ExternalData& externalData) {
	for (const auto value : context.wave) {
		blockIntermediate += value * value;
		blockCounter++;
		if (blockCounter == blockSize) {
			pushMicroBlock(blockIntermediate);
			blockCounter = 0;
			blockIntermediate = 0.0;
		}
	}
}

void Loudness::pushMicroBlock(double value) {
	blocks[static_cast<size_t>(nextBlockIndex)] = value;
	nextBlockIndex++;
	if (nextBlockIndex >= blocksCount) {
		nextBlockIndex = 0;
	}

	const double gatingValue = prevValue * gatingValueCoefficient;

	tempBlocks = blocks;
	std::sort(tempBlocks.begin(), tempBlocks.end(), std::greater<>());

	double newValue = 0.0;
	index counter = 0;
	for (const auto blockEnergy : blocks) {
		if (params.ignoreGatingForSilence && blockEnergy == 0.0
			|| blockEnergy >= gatingValue || counter < minBlocksCount) {
			newValue += blockEnergy;
			counter++;
		}
	}
	newValue /= static_cast<double>(counter);
	newValue *= blockNormalizer;

	pushNextValue(newValue);
}

void Loudness::pushNextValue(double value) {
	prevValue = value;

	auto result = pushLayer(0);
	result[0] = params.transformer.apply(static_cast<float>(value));
}
