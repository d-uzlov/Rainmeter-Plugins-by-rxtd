// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "rxtd/audio_analyzer/audio_utils/RandomGenerator.h"

#include "FftAnalyzer.h"

using rxtd::audio_analyzer::handler::FftAnalyzer;
using rxtd::audio_analyzer::handler::HandlerBase;
using ParamsContainer = HandlerBase::ParamsContainer;

ParamsContainer FftAnalyzer::vParseParams(ParamParseContext& context) const noexcept(false) {
	Params params;

	params.binWidth = context.parser.parseFloat(context.options.get(L"binWidth"), 100.0);
	if (params.binWidth <= 0.0) {
		context.log.error(L"binWidth must be > 0 but {} found", params.binWidth);
		throw InvalidOptionsException{};
	}
	if (params.binWidth <= 1.0) {
		context.log.warning(L"BinWidth {} is dangerously small, use values > 1", params.binWidth);
	}

	double overlapBoost = context.parser.parseFloat(context.options.get(L"overlapBoost"), 2.0);
	overlapBoost = std::max(overlapBoost, 1.0);
	params.overlap = (overlapBoost - 1.0) / overlapBoost;

	params.cascadesCount = context.parser.parseInt(context.options.get(L"cascadesCount"), 5);
	if (params.cascadesCount <= 0) {
		context.log.warning(L"cascadesCount must be in range [1, 20] but {} found. Assume 1", params.cascadesCount);
		params.cascadesCount = 1;
	} else if (params.cascadesCount > 20) {
		context.log.warning(L"cascadesCount must be in range [1, 20] but {} found. Assume 20", params.cascadesCount);
		params.cascadesCount = 20;
	}

	params.randomTest = std::abs(context.parser.parseFloat(context.options.get(L"testRandom"), 0.0));
	params.randomDuration = std::abs(context.parser.parseFloat(context.options.get(L"randomDuration"), 1000.0)) * 0.001;

	params.wcfDescription = context.options.get(L"windowFunction").asString(L"hann");
	params.createWindow = fft_utils::WindowFunctionHelper::parse(params.wcfDescription, context.parser, context.log.context(L"windowFunction: "));

	return params;
}

HandlerBase::ConfigurationResult
FftAnalyzer::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();

	fftSize = kiss_fft::calculateNextFastSize(static_cast<index>(static_cast<double>(config.sampleRate) / params.binWidth), true);

	constexpr index minFftSize = 16;

	fftSize = std::max<index>(fftSize, minFftSize);

	std::vector<float> window;
	window.resize(static_cast<size_t>(fftSize));
	params.createWindow(window);
	fft.setParams(fftSize, std::move(window));

	inputStride = static_cast<index>(static_cast<double>(fftSize) * (1.0 - params.overlap));
	inputStride = std::clamp<index>(inputStride, minFftSize, fftSize);

	randomBlockSize = static_cast<index>(params.randomDuration * static_cast<double>(config.sampleRate) * static_cast<double>(fftSize) / static_cast<double>(inputStride));
	randomCurrentOffset = 0;

	cascades.resize(static_cast<size_t>(params.cascadesCount));

	fft_utils::FftCascade::Params cascadeParams;
	cascadeParams.fftSize = fftSize;
	cascadeParams.samplesPerSec = config.sampleRate;
	cascadeParams.inputStride = inputStride;
	cascadeParams.callback = [this](array_view<float> result, index cascade) {
		pushLayer(cascade).copyFrom(result);
	};

	for (index i = 0; i < params.cascadesCount; i++) {
		const auto next = i + 1 < static_cast<index>(cascades.size()) ? &cascades[static_cast<size_t>(i + 1)] : nullptr;
		cascades[static_cast<size_t>(i)].setParams(cascadeParams, &fft, next, i);
	}


	auto& snapshot = externalData.clear<Snapshot>();

	snapshot.fftSize = fftSize;
	snapshot.sampleRate = getConfiguration().sampleRate;
	snapshot.cascadesCount = static_cast<index>(cascades.size());

	std::vector<index> eqWS;
	index equivalentSize = inputStride;
	for (int i = 0; i < params.cascadesCount; ++i) {
		eqWS.push_back(equivalentSize);
		equivalentSize *= 2;
	}

	return { fftSize / 2, std::move(eqWS) };
}

void FftAnalyzer::vProcess(ProcessContext context, ExternalData& externalData) {
	if (params.randomTest != 0.0) {
		processRandom(context.wave.size(), context.killTime);
	} else {
		cascades[0].process(context.wave, context.killTime);
	}
}

bool FftAnalyzer::getProp(
	const Snapshot& snapshot,
	isview prop,
	BufferPrinter& printer,
	const ExternalMethods::CallContext& context
) {
	return false;
}

void FftAnalyzer::processRandom(index waveSize, clock::time_point killTime) {
	audio_utils::RandomGenerator random;

	std::vector<float> wave;
	wave.reserve(static_cast<size_t>(waveSize));

	for (index i = 0; i < waveSize; ++i) {
		if (randomState == RandomState::eON) {
			wave.push_back(static_cast<float>(random.next() * params.randomTest));
		} else {
			wave.push_back(0.0f);
		}

		randomCurrentOffset++;
		if (randomCurrentOffset == randomBlockSize) {
			randomCurrentOffset = 0;
			if (randomState == RandomState::eON) {
				randomState = RandomState::eOFF;
			} else {
				randomState = RandomState::eON;
			}
		}
	}

	cascades[0].process(wave, killTime);
}
