/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"

#include "undef.h"
#include <numeric>

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<BlockHandler::Params> BlockHandler::parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger& cl) {
	Params params;
	params.attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0);
	params.decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.attackTime), 0.0);

	params.attackTime *= 0.001;
	params.decayTime *= 0.001;

	params.resolution = optionMap.get(L"resolution"sv).asFloat(10);
	if (params.resolution <= 0) {
		cl.warning(L"block must be > 0 but {} found. Assume 10", params.resolution);
		params.resolution = 10;
	}
	params.resolution *= 0.001;

	params.subtractMean = optionMap.get(L"subtractMean").asBool(true);

	return params;
}

void BlockHandler::setNextValue(double value) {
	filter.next(value);
}

void BlockHandler::setParams(Params params) {
	if (this->params == params) {
		return;
	}

	this->params = params;

	recalculateConstants();
}

void BlockHandler::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	recalculateConstants();

	_setSamplesPerSec(samplesPerSec);
}

const wchar_t* BlockHandler::getProp(const isview& prop) const {
	if (prop == L"block size") {
		propString = std::to_wstring(blockSize);
	} else if (prop == L"attack") {
		propString = std::to_wstring(params.attackTime * 1000.0);
	} else if (prop == L"decay") {
		propString = std::to_wstring(params.decayTime * 1000.0);
	} else {
		return nullptr;
	}
	return propString.c_str();
}

void BlockHandler::reset() {
	counter = 0;
	result = 0.0;
	filter.reset();
	_reset();
}

void BlockHandler::process(const DataSupplier& dataSupplier) {
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

	filter.setParams(params.attackTime, params.decayTime, samplesPerSec, blockSize);
}

void BlockHandler::processSilence(const DataSupplier& dataSupplier) {
	const auto waveSize = dataSupplier.getWave().size();
	index waveProcessed = 0;

	while (waveProcessed != waveSize) {
		const index missingPoints = blockSize - counter;
		if (waveProcessed + missingPoints <= waveSize) {
			finishBlock();
			waveProcessed += missingPoints;
		} else {
			const auto waveRemainder = waveSize - waveProcessed;
			counter = waveRemainder;
			break;
		}
	}
}

void BlockHandler::finish(const DataSupplier& dataSupplier) {
	result = filter.getLastResult();
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
