/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"
#include <numeric>
#include "option-parser/OptionMap.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<BlockHandler::Params> BlockHandler::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params;

	params.resolution = optionMap.get(L"resolution"sv).asFloat(1000.0 / 60.0);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 10", params.resolution);
		params.resolution = 10;
	}
	params.resolution *= 0.001;

	params.subtractMean = optionMap.get(L"subtractMean").asBool(true);

	params.transform = optionMap.get(L"transform").asString();
	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), cl);

	// legacy
	if (optionMap.has(L"attack") || optionMap.has(L"decay")) {
		cl.notice(L"Using deprecated 'attack'/'decay' options. Transforms are ignored");
		params.legacy_attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0);
		params.legacy_decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.legacy_attackTime), 0.0);

		utils::BufferPrinter printer;
		printer.print(L"filter[a {}, d {}]", params.legacy_attackTime, params.legacy_decayTime);
		params.transformer = audio_utils::TransformationParser::parse(utils::Option{ printer.getBufferView() }, cl);
	}

	return params;
}

void BlockHandler::setNextValue(double value) {
	result = params.transformer.apply(value);
}

void BlockHandler::setParams(Params _params, Channel channel) {
	if (params == _params) {
		return;
	}

	params = std::move(_params);

	recalculateConstants();

	if (params.transformer.isEmpty()) {
		params.transform = getDefaultTransform();
		utils::Rainmeter::Logger dummyLogger;
		params.transformer = audio_utils::TransformationParser::parse(utils::Option{ params.transform }, dummyLogger);
	}
}

void BlockHandler::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	recalculateConstants();

	_setSamplesPerSec(samplesPerSec);
}

bool BlockHandler::getProp(const isview& prop, utils::BufferPrinter& printer) const {
	if (prop == L"block size") {
		printer.print(blockSize);
		return true;
	}
	if (prop == L"attack") {
		printer.print(params.legacy_attackTime);
		return true;
	}
	if (prop == L"decay") {
		printer.print(params.legacy_decayTime);
		return true;
	}

	if (prop == L"transformString") {
		printer.print(params.transform);
		return true;
	}

	return false;
}

void BlockHandler::reset() {
	counter = 0;
	result = 0.0;
	params.transformer.resetState();
	_reset();
}

void BlockHandler::_process(const DataSupplier& dataSupplier) {
	auto wave = dataSupplier.getWave();

	float mean = 0.0;
	if (isAverageNeeded()) {
		mean = std::accumulate(wave.begin(), wave.end(), 0.0f) / wave.size();
	}

	_process(wave, mean);
}

void BlockHandler::recalculateConstants() {
	auto test = samplesPerSec * params.resolution;
	blockSize = static_cast<decltype(blockSize)>(test);
	if (blockSize < 1) {
		blockSize = 1;
	}

	params.transformer.setParams(samplesPerSec, blockSize);
}

void BlockHandler::_processSilence(const DataSupplier& dataSupplier) {
	const auto waveSize = dataSupplier.getWave().size();
	index waveProcessed = 0;

	while (waveProcessed != waveSize) {
		const index missingPoints = blockSize - counter;
		if (waveProcessed + missingPoints <= waveSize) {
			counter = blockSize;
			finishBlock();
			waveProcessed += missingPoints;
		} else {
			const auto waveRemainder = waveSize - waveProcessed;
			counter = waveRemainder;
			break;
		}
	}
}

void BlockRms::_process(array_view<float> wave, float average) {
	for (double x : wave) {
		x -= average;
		intermediateResult += x * x;
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}
}

void BlockRms::finishBlock() {
	const double value = std::sqrt(intermediateResult / getBlockSize());
	setNextValue(value);
	counter = 0;
	intermediateResult = 0.0;
}

void BlockRms::_reset() {
	intermediateResult = 0.0;
}

sview BlockRms::getDefaultTransform() {
	return L"db map[from -70 : 0] clamp filter[a 200, d 200]"sv;
}

void BlockPeak::_process(array_view<float> wave, float average) {
	for (double x : wave) {
		x -= average;
		intermediateResult = std::max<double>(intermediateResult, std::abs(x));
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}
}

void BlockPeak::finishBlock() {
	setNextValue(intermediateResult);
	counter = 0;
	intermediateResult = 0.0;
}

void BlockPeak::_reset() {
	intermediateResult = 0.0;
}

sview BlockPeak::getDefaultTransform() {
	return L"filter[a 0, d 500]"sv;
}
