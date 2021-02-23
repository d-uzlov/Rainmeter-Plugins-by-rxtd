/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <random>
#include "audio-utils/RandomGenerator.h"

#include "FftAnalyzer.h"

using rxtd::audio_analyzer::handler::FftAnalyzer;
using rxtd::audio_analyzer::handler::HandlerBase;

rxtd::audio_analyzer::handler::ParamsContainer FftAnalyzer::vParseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	Version version
) const {
	ParamsContainer result;
	auto& params = result.clear<Params>();

	params.binWidth = om.get(L"binWidth").asFloat(100.0);
	if (params.binWidth <= 0.0) {
		cl.error(L"binWidth must be > 0 but {} found", params.binWidth);
		return {};
	}
	if (params.binWidth <= 1.0) {
		cl.warning(L"BinWidth {} is dangerously small, use values > 1", params.binWidth);
	}

	if (om.has(L"overlapBoost")) {
		double overlapBoost = om.get(L"overlapBoost").asFloat(2.0);
		overlapBoost = std::max(overlapBoost, 1.0);
		params.overlap = (overlapBoost - 1.0) / overlapBoost;
	} else {
		params.overlap = std::clamp(om.get(L"overlap").asFloat(0.5), 0.0, 1.0);
	}

	params.cascadesCount = om.get(L"cascadesCount").asInt(5);
	if (params.cascadesCount <= 0) {
		cl.warning(L"cascadesCount must be in range [1, 20] but {} found. Assume 1", params.cascadesCount);
		params.cascadesCount = 1;
	} else if (params.cascadesCount > 20) {
		cl.warning(L"cascadesCount must be in range [1, 20] but {} found. Assume 20", params.cascadesCount);
		params.cascadesCount = 20;
	}

	params.randomTest = std::abs(om.get(L"testRandom").asFloat(0.0));
	params.randomDuration = std::abs(om.get(L"randomDuration").asFloat(1000.0)) * 0.001;

	params.wcfDescription = om.get(L"windowFunction").asString(L"hann");
	params.createWindow = audio_utils::WindowFunctionHelper::parse(params.wcfDescription, cl);

	return result;
}

HandlerBase::ConfigurationResult
FftAnalyzer::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();

	fftSize = kiss_fft::calculateNextFastSize(static_cast<index>(static_cast<double>(config.sampleRate) / params.binWidth), true);

	constexpr index minFftSize = 16;

	fftSize = std::max<index>(fftSize, minFftSize);

	fft.setParams(fftSize, params.createWindow(fftSize));

	inputStride = static_cast<index>(static_cast<double>(fftSize) * (1.0 - params.overlap));
	inputStride = std::clamp<index>(inputStride, minFftSize, fftSize);

	randomBlockSize = static_cast<index>(params.randomDuration * static_cast<double>(config.sampleRate) * static_cast<double>(fftSize) / static_cast<double>(inputStride));
	randomCurrentOffset = 0;

	cascades.resize(params.cascadesCount);

	audio_utils::FftCascade::Params cascadeParams;
	cascadeParams.fftSize = fftSize;
	cascadeParams.samplesPerSec = config.sampleRate;
	cascadeParams.inputStride = inputStride;
	cascadeParams.callback = [this](array_view<float> result, index cascade) {
		pushLayer(cascade).copyFrom(result);
	};

	for (index i = 0; i < params.cascadesCount; i++) {
		const auto next = i + 1 < index(cascades.size()) ? &cascades[i + 1] : nullptr;
		cascades[i].setParams(cascadeParams, &fft, next, i);
	}


	auto& snapshot = externalData.clear<Snapshot>();

	snapshot.fftSize = fftSize;
	snapshot.sampleRate = getConfiguration().sampleRate;
	snapshot.cascadesCount = cascades.size();

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
	wave.reserve(waveSize);

	for (index i = 0; i < waveSize; ++i) {
		if (randomState == RandomState::ON) {
			wave.push_back(float(random.next() * params.randomTest));
		} else {
			wave.push_back(0.0f);
		}

		randomCurrentOffset++;
		if (randomCurrentOffset == randomBlockSize) {
			randomCurrentOffset = 0;
			if (randomState == RandomState::ON) {
				randomState = RandomState::OFF;
			} else {
				randomState = RandomState::ON;
			}
		}
	}

	cascades[0].process(wave, killTime);
}
