/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockMean.h"
#include <cmath>

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

void rxaa::BlockMean::setParams(Params params) {
	this->params = params;

	recalculateConstants();
}

const double* rxaa::BlockMean::getData() const {
	return &result;
}

index rxaa::BlockMean::getCount() const {
	return 0;
}

void rxaa::BlockMean::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;

	recalculateConstants();
}

const wchar_t* rxaa::BlockMean::getProp(const isview& prop) {
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

void rxaa::BlockMean::reset() {
	counter = 0;
	intermediateResult = 0.0;
	result = 0.0;
}

void rxaa::BlockMean::recalculateConstants() {
	blockSize = static_cast<decltype(blockSize)>(samplesPerSec * params.resolution);

	attackDecayConstants[0] = calculateAttackDecayConstant(params.attackTime, samplesPerSec, blockSize);
	attackDecayConstants[1] = calculateAttackDecayConstant(params.decayTime, samplesPerSec, blockSize);

	reset();
}

void rxaa::BlockRms::process(const DataSupplier& dataSupplier) {
	const auto wave = dataSupplier.getWave();
	for (index frame = 0; frame < dataSupplier.getWaveSize(); ++frame) {
		const double x = wave[frame];
		intermediateResult += x * x;
		counter++;
		if (counter >= blockSize) {
			finish();
		}
	}
}

void rxaa::BlockMean::processSilence(const DataSupplier& dataSupplier) {
	const auto waveSize = dataSupplier.getWaveSize();
	auto waveProcessed = 0u;

	while (waveProcessed != waveSize) {
		const auto missingPoints = blockSize - counter;
		if (waveProcessed + missingPoints <= waveSize) {
			finish();
			waveProcessed += missingPoints;
		} else {
			const auto waveRemainder = waveSize - waveProcessed;
			counter = waveRemainder;
			break;
		}
	}
}

std::optional<rxaa::BlockMean::Params> rxaa::BlockMean::parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger& cl) {
	Params params;
	params.attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0) * 0.001;
	params.decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.attackTime), 0.0) * 0.001;

	params.resolution = optionMap.get(L"resolution"sv).asFloat(10);
	if (params.resolution <= 0) {
		cl.warning(L"block must be > 0 but {} found. Assume 10", params.resolution);
		params.resolution = 10;
	}
	params.resolution *= 0.001;

	return params;
}

void rxaa::BlockRms::finish() {
	const double value = std::sqrt(intermediateResult / blockSize);
	result = value + attackDecayConstants[(value < result)] * (result - value);
	counter = 0;
	intermediateResult = 0.0;
}

void rxaa::BlockPeak::process(const DataSupplier& dataSupplier) {
	const auto wave = dataSupplier.getWave();
	const auto waveSize = dataSupplier.getWaveSize();
	for (index frame = 0; frame < waveSize; ++frame) {
		intermediateResult = std::max(intermediateResult, static_cast<double>(std::abs(wave[frame])));
		counter++;
		if (counter >= blockSize) {
			finish();
		}
	}
}

void rxaa::BlockPeak::finish() {
	result = intermediateResult + attackDecayConstants[(intermediateResult < result)] * (result - intermediateResult);
	counter = 0;
	intermediateResult = 0.0;
}
