/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"
#include <numeric>
#include "option-parser/OptionMap.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

bool BlockHandler::parseParams(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, void* paramsPtr) const {
	auto& params = *static_cast<Params*>(paramsPtr);

	params.resolution = optionMap.get(L"resolution"sv).asFloat(1000.0 / 60.0);
	if (params.resolution <= 0) {
		cl.warning(L"resolution must be > 0 but {} found. Assume 10", params.resolution);
		params.resolution = 10;
	}
	params.resolution *= 0.001;

	params.subtractMean = optionMap.get(L"subtractMean").asBool(true);

	params.transform = optionMap.get(L"transform").asString();
	auto transformLogger = cl.context(L"transform: ");
	params.transformer = audio_utils::TransformationParser::parse(optionMap.get(L"transform"), transformLogger);

	params.legacy_attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0);
	params.legacy_decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.legacy_attackTime), 0.0);
	
	// legacy
	if (params.legacy_attackTime != 0.0 || params.legacy_decayTime != 0.0) {
		cl.notice(L"Using deprecated 'attack'/'decay' options. Transforms are ignored");

		utils::BufferPrinter printer;
		printer.print(L"filter[attack {}, decay {}]", params.legacy_attackTime, params.legacy_decayTime);
		params.transformer = audio_utils::TransformationParser::parse(utils::Option{ printer.getBufferView() }, cl);
	}

	return true;
}

void BlockHandler::setParams(const Params& value) {
	params = value;

	if (params.transformer.isEmpty()) {
		params.transform = getDefaultTransform();
		utils::Rainmeter::Logger dummyLogger;
		params.transformer = audio_utils::TransformationParser::parse(utils::Option{ params.transform }, dummyLogger);
	}
}

SoundHandler::LinkingResult BlockHandler::vFinishLinking(Logger& cl) {
	blockSize = static_cast<decltype(blockSize)>(getSampleRate() * params.resolution);
	if (blockSize < 1) {
		blockSize = 1;
	}

	params.transformer.setParams(getSampleRate(), blockSize);

	return { 1, 1 };
}

void BlockHandler::vReset() {
	counter = 0;
	params.transformer.resetState();
	_reset();
}

void BlockHandler::vProcess(const DataSupplier& dataSupplier) {
	auto wave = dataSupplier.getWave();

	float mean = 0.0;
	if (params.subtractMean && isAverageNeeded()) {
		mean = std::accumulate(wave.begin(), wave.end(), 0.0f) / wave.size();
	}

	_process(wave, mean);
}

bool BlockHandler::vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
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

void BlockHandler::setNextValue(double value) {
	generateLayerData(0)[0] = params.transformer.apply(float(value));
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

sview BlockRms::getDefaultTransform() const {
	return L"db map[from -70 : 0] filter[attack 200, decay 200] clamp"sv;
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

sview BlockPeak::getDefaultTransform() const {
	return L"filter[attack 0, decay 500] clamp"sv;
}
