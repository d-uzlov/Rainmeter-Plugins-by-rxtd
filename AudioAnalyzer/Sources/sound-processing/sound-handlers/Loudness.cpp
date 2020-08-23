/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Loudness.h"

#include "MyMath.h"
#include "option-parser/OptionMap.h"

using namespace audio_analyzer;

SoundHandler::ParseResult Loudness::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(om.get(L"transform").asString(), transformLogger);

	params.gatingLimit = om.get(L"gatingLimit").asFloat(0.5);
	params.gatingLimit = std::clamp(params.gatingLimit, 0.0, 1.0);

	params.updatesPerSecond = om.get(L"updatesPerSecond").asFloat(20.0);
	params.updatesPerSecond = std::clamp(params.updatesPerSecond, 0.01, 60.0);

	params.timeWindowMs = om.get(L"timeWindow").asFloat(1000.0);
	params.timeWindowMs = std::clamp(params.timeWindowMs, 0.01, 10000.0);

	params.gatingDb = om.get(L"gatingDb").asFloat(-20.0);
	params.gatingDb = std::clamp(params.gatingDb, -70.0, 0.0);

	params.ignoreGatingForSilence = om.get(L"ignoreGatingForSilence").asBool(true);

	ParseResult result;
	result.setParams(std::move(params));
	return result;
}

SoundHandler::ConfigurationResult Loudness::vConfigure(Logger& cl) {
	blocksCount = index(params.timeWindowMs / 1000.0 * params.updatesPerSecond);
	blocksCount = std::max<index>(blocksCount, 0);

	auto& config = getConfiguration();
	const index sampleRate = config.sampleRate;

	blockSize = index(sampleRate / params.updatesPerSecond);
	blockNormalizer = 1.0 / blockSize;
	blockCounter = 0;

	params.transformer.setParams(sampleRate, blockSize);

	blocks.reset(blocksCount, 0.0);
	blocks.setMaxSize(blocksCount * 5);
	prevValue = 0.0;

	gatingValueCoefficient = utils::MyMath::db2amplitude(params.gatingDb) * blockSize;

	minBlocksCount = index(blocksCount * (1.0 - params.gatingLimit));

	return { 1, 1 };
}

void Loudness::vProcess(array_view<float> wave, clock::time_point killTime) {
	for (const auto value : wave) {
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
	blocks.removeFirst(1);
	blocks.allocateNext(1)[0] = value;

	const double gatingValue = prevValue * gatingValueCoefficient;

	double newValue = 0.0;
	index counter = 0;
	for (const auto blockEnergy : blocks.getLast(blocksCount)) {
		if (!(params.ignoreGatingForSilence && blockEnergy == 0.0) && blockEnergy < gatingValue) {
			continue;
		}
		newValue += blockEnergy;
		counter++;
	}
	if (counter != 0) {
		counter = std::max(counter, minBlocksCount);
		newValue /= counter;
		newValue *= blockNormalizer;
	}

	pushNextValue(newValue);
}

void Loudness::pushNextValue(double value) {
	prevValue = value;

	auto result = pushLayer(0, blockSize);
	result[0] = params.transformer.apply(float(value));
}
