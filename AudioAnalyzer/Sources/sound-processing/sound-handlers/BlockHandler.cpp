/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"

using namespace audio_analyzer;

SoundHandler::ParseResult BlockHandler::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	ParseResult result{ true };
	auto& params = result.params.clear<Params>();

	if (om.has(L"resolution")) {
		params.updateInterval = om.get(L"resolution").asFloat(10.0);
		if (params.updateInterval <= 0) {
			cl.warning(L"resolution must be > 0 but {} found. Assume 10", params.updateInterval);
			params.updateInterval = 10;
		}
		params.updateInterval *= 0.001;
	} else {
		double updateRate = om.get(L"UpdateRate").asFloat(60.0);
		updateRate = std::clamp(updateRate, 0.01, 500.0);
		params.updateInterval = 1.0 / updateRate;
	}

	const double defaultAttack = legacyNumber < 104 ? 100.0 : 0.0;
	params.attackTime = std::max(om.get(L"attack").asFloat(defaultAttack), 0.0);
	params.decayTime = std::max(om.get(L"decay").asFloat(params.attackTime), 0.0);
	params.attackTime *= 0.001;
	params.decayTime *= 0.001;

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = CVT::parse(om.get(L"transform").asString(), transformLogger);

	result.externalMethods.getProp = wrapExternalMethod<Snapshot, &getProp>();
	return result;
}

SoundHandler::ConfigurationResult
BlockHandler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	blockSize = static_cast<decltype(blockSize)>(config.sampleRate * params.updateInterval);
	blockSize = std::max<index>(blockSize, 1);

	filter.setParams(params.attackTime, params.decayTime, config.sampleRate, blockSize);

	auto& snapshot = externalData.clear<Snapshot>();

	snapshot.blockSize = blockSize;

	return { 1, { blockSize } };
}

void BlockHandler::vProcess(ProcessContext context, ExternalData& externalData) {
	_process(context.wave);
}

void BlockHandler::setNextValue(float value) {
	value = filter.next(value);
	pushLayer(0)[0] = params.transformer.apply(value);
}

bool BlockHandler::getProp(
	const Snapshot& snapshot,
	isview prop,
	BufferPrinter& printer,
	const ExternCallContext& context
) {
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
