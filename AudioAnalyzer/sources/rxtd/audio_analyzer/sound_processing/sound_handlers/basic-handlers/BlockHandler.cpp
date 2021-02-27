/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BlockHandler.h"

using rxtd::audio_analyzer::handler::BlockHandler;
using rxtd::audio_analyzer::handler::HandlerBase;
using rxtd::audio_analyzer::handler::BlockRms;
using rxtd::audio_analyzer::handler::BlockPeak;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer BlockHandler::vParseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	Version version
) const {
	ParamsContainer result;
	auto& params = result.clear<Params>();

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

	params.attackTime = std::max(om.get(L"attack").asFloat(0), 0.0);
	params.decayTime = std::max(om.get(L"decay").asFloat(params.attackTime), 0.0);
	params.attackTime *= 0.001;
	params.decayTime *= 0.001;

	auto transformLogger = cl.context(L"transform: ");
	params.transformer = CVT::parse(om.get(L"transform").asString(), transformLogger);

	return result;
}

HandlerBase::ConfigurationResult
BlockHandler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	blockSize = static_cast<decltype(blockSize)>(static_cast<double>(config.sampleRate) * params.updateInterval);
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
	const ExternalMethods::CallContext& context
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
	const float value = std::sqrt(static_cast<float>(intermediateResult) / static_cast<float>(getBlockSize()));
	setNextValue(value);
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
