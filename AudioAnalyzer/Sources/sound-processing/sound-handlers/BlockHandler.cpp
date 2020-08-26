/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"
#include "option-parser/OptionMap.h"

using namespace audio_analyzer;

SoundHandler::ParseResult BlockHandler::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	Params params;

	const sview updateIntervalOptionName = om.has(L"updateInterval") ? L"updateInterval" : L"resolution";
	params.updateIntervalMs = om.get(updateIntervalOptionName).asFloat(1000.0 / 60.0);
	if (params.updateIntervalMs <= 0) {
		cl.warning(L"{} must be > 0 but {} found. Assume 10", updateIntervalOptionName, params.updateIntervalMs);
		params.updateIntervalMs = 10;
	}
	params.updateIntervalMs *= 0.001;

	if (legacyNumber < 104) {
		params.legacy_attackTime = std::max(om.get(L"attack").asFloat(100), 0.0);
		params.legacy_decayTime = std::max(om.get(L"decay").asFloat(params.legacy_attackTime), 0.0);

		utils::BufferPrinter printer;
		printer.print(L"filter[attack {}, decay {}]", params.legacy_attackTime, params.legacy_decayTime);
		params.transformer = audio_utils::TransformationParser::parse(printer.getBufferView(), cl);
	} else {
		params.legacy_attackTime = 0.0;
		params.legacy_decayTime = 0.0;

		auto transformLogger = cl.context(L"transform: ");
		params.transformer = audio_utils::TransformationParser::parse(om.get(L"transform").asString(), transformLogger);
	}

	ParseResult result;
	result.setParams(std::move(params));
	result.setPropGetter(getProp);
	return result;
}

SoundHandler::ConfigurationResult BlockHandler::vConfigure(const std::any& _params, Logger& cl) {
	params = std::any_cast<Params>(_params);

	auto& config = getConfiguration();
	blockSize = static_cast<decltype(blockSize)>(config.sampleRate * params.updateIntervalMs);
	blockSize = std::max<index>(blockSize, 1);

	params.transformer.setParams(config.sampleRate, blockSize);

	return { 1, 1 };
}

void BlockHandler::vProcess(array_view<float> wave, clock::time_point killTime) {
	_process(wave);
}

void BlockHandler::setNextValue(float value) {
	pushLayer(0, blockSize)[0] = params.transformer.apply(value);
}

void BlockHandler::vConfigureSnapshot(std::any& handlerSpecificData) const {
	auto snapshotPtr = std::any_cast<Snapshot>(&handlerSpecificData);
	if (snapshotPtr == nullptr) {
		handlerSpecificData = Snapshot{ };
		snapshotPtr = std::any_cast<Snapshot>(&handlerSpecificData);
	}
	auto& snapshot = *snapshotPtr;

	snapshot.blockSize = blockSize;
}

bool BlockHandler::getProp(const std::any& handlerSpecificData, isview prop, utils::BufferPrinter& printer) {
	auto& snapshot = *std::any_cast<Snapshot>(&handlerSpecificData);

	if (prop == L"block size") {
		printer.print(snapshot.blockSize);
		return true;
	}

	return false;
}

void BlockRms::_process(array_view<float> wave) {
	for (double x : wave) {
		intermediateResult += x * x;
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}
}

void BlockRms::finishBlock() {
	const double value = std::sqrt(intermediateResult / getBlockSize());
	setNextValue(float(value));
	counter = 0;
	intermediateResult = 0.0;
}

void BlockPeak::_process(array_view<float> wave) {
	for (float x : wave) {
		intermediateResult = std::max(intermediateResult, std::abs(x));
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
