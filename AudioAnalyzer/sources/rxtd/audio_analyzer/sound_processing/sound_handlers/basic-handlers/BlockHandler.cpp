// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "BlockHandler.h"

using rxtd::audio_analyzer::handler::BlockHandler;
using rxtd::audio_analyzer::handler::HandlerBase;
using rxtd::audio_analyzer::handler::BlockRms;
using rxtd::audio_analyzer::handler::BlockPeak;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer BlockHandler::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	double updateRate = context.parser.parseFloat(context.options.get(L"UpdateRate"), 60.0);
	updateRate = std::clamp(updateRate, 0.01, 500.0);
	params.updateInterval = 1.0 / updateRate;

	params.attackTime = std::max(context.parser.parseFloat(context.options.get(L"attack"), 0.0), 0.0);
	params.decayTime = std::max(context.parser.parseFloat(context.options.get(L"decay"), params.attackTime), 0.0);
	params.attackTime *= 0.001;
	params.decayTime *= 0.001;

	auto transformLogger = context.log.context(L"transform: ");
	params.transformer = CVT::parse(context.options.get(L"transform").asString(), context.parser, transformLogger);

	return params;
}

HandlerBase::ConfigurationResult
BlockHandler::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	blockSize = static_cast<index>(static_cast<double>(config.sampleRate) * params.updateInterval);
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
	for (auto x : wave) {
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
	for (auto x : wave) {
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
