/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <random>
#include "../../../audio-utils/RandomGenerator.h"

#include "FftAnalyzer.h"

using namespace audio_analyzer;

SoundHandler::ParseResult FftAnalyzer::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	ParseResult result{ true };
	auto& params = result.params.clear<Params>();

	if (legacyNumber < 104) {
		params.legacy_attackTime = std::max(om.get(L"attack").asFloat(100), 0.0);
		params.legacy_decayTime = std::max(om.get(L"decay").asFloat(params.legacy_attackTime), 0.0);

		params.legacy_attackTime *= 0.001;
		params.legacy_decayTime *= 0.001;
	} else {
		params.legacy_attackTime = 0.0;
		params.legacy_decayTime = 0.0;
	}

	if (legacyNumber < 104) {
		if (const auto sizeBy = om.get(L"sizeBy").asIString(L"binWidth");
			sizeBy == L"binWidth") {
			params.binWidth = om.get(L"binWidth").asFloat(100.0);
			if (params.binWidth <= 0.0) {
				cl.error(L"binWidth must be > 0 but {} found", params.binWidth);
				return {};
			}
			if (params.binWidth <= 1.0) {
				cl.warning(L"BinWidth {} is dangerously small, use values > 1", params.binWidth);
			}
			params.legacy_sizeBy = SizeBy::BIN_WIDTH;
		} else {
			if (sizeBy == L"size") {
				params.binWidth = om.get(L"size").asInt(1000);
				if (params.binWidth < 2) {
					cl.warning(L"Size must be >= 2 but {} found. Assume 1000", params.binWidth);
					params.binWidth = 1000;
				}
				params.legacy_sizeBy = SizeBy::SIZE;

			} else if (sizeBy == L"sizeExact") {
				params.binWidth = om.get(L"size").asInt(1000);
				if (params.binWidth < 2) {
					cl.error(L"Size must be >= 2, must be even, but {} found", params.binWidth);
					return {};
				}
				params.legacy_sizeBy = SizeBy::SIZE_EXACT;
			} else {
				cl.error(L"Unknown fft sizeBy '{}'", sizeBy);
				return {};
			}
		}
	} else {
		params.binWidth = om.get(L"binWidth").asFloat(100.0);
		if (params.binWidth <= 0.0) {
			cl.error(L"binWidth must be > 0 but {} found", params.binWidth);
			return {};
		}
		if (params.binWidth <= 1.0) {
			cl.warning(L"BinWidth {} is dangerously small, use values > 1", params.binWidth);
		}
		params.legacy_sizeBy = SizeBy::BIN_WIDTH;
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

	if (legacyNumber < 104) {
		params.legacy_correctZero = om.get(L"correctZero").asBool(true);
		params.legacyAmplification = true;
	} else {
		params.legacy_correctZero = false;
		params.legacyAmplification = false;
	}

	params.wcfDescription = om.get(L"windowFunction").asString(L"hann");
	params.wcf = audio_utils::WindowFunctionHelper::parse(params.wcfDescription, cl);

	result.externalMethods.getProp = wrapExternalMethod<Snapshot, &getProp>();
	return result;
}

SoundHandler::ConfigurationResult
FftAnalyzer::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();

	switch (params.legacy_sizeBy) {
	case SizeBy::BIN_WIDTH:
		fftSize = kiss_fft::calculateNextFastSize(index(config.sampleRate / params.binWidth), true);
		break;
	case SizeBy::SIZE:
		fftSize = kiss_fft::calculateNextFastSize(index(params.binWidth), true);

		if (fftSize <= 1) {
			cl.error(L"fft size is too small");
			return {};
		}

		break;
	case SizeBy::SIZE_EXACT:
		fftSize = static_cast<index>(static_cast<size_t>(params.binWidth) & ~1); // only even sizes are allowed

		if (fftSize <= 1) {
			cl.error(L"fft size is too small");
			return {};
		}

		break;
	}

	constexpr index minFftSize = 16;

	fftSize = std::max<index>(fftSize, minFftSize);

	fft.setParams(fftSize, !params.legacyAmplification, params.wcf(fftSize));

	inputStride = static_cast<index>(fftSize * (1 - params.overlap));
	inputStride = std::clamp<index>(inputStride, minFftSize, fftSize);

	randomBlockSize = index(params.randomDuration * config.sampleRate * fftSize / inputStride);
	randomCurrentOffset = 0;

	cascades.resize(params.cascadesCount);

	audio_utils::FftCascade::Params cascadeParams;
	cascadeParams.fftSize = fftSize;
	cascadeParams.samplesPerSec = config.sampleRate;
	cascadeParams.legacy_attackTime = params.legacy_attackTime;
	cascadeParams.legacy_decayTime = params.legacy_decayTime;
	cascadeParams.inputStride = inputStride;
	cascadeParams.legacy_correctZero = params.legacy_correctZero;
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

	auto& snapshot = externalData.cast<Snapshot>();
	snapshot.dc.clear();

	for (const auto& cascade : cascades) {
		snapshot.dc.push_back(float(cascade.legacy_getDC()));
	}
}

bool FftAnalyzer::getProp(
	const Snapshot& snapshot,
	isview prop,
	BufferPrinter& printer,
	const ExternCallContext& context
) {
	if (prop == L"size") {
		printer.print(snapshot.fftSize);
		return true;
	}

	auto cascadeIndex = legacy_parseIndexProp(prop, L"nyquist frequency", snapshot.cascadesCount + 1);
	if (cascadeIndex == -2) {
		return L"0";
	}
	if (cascadeIndex >= 0) {
		if (cascadeIndex > 0) {
			cascadeIndex--;
		}
		printer.print(static_cast<index>(snapshot.sampleRate / 2.0 / std::pow(2, cascadeIndex)));
		return true;
	}

	cascadeIndex = legacy_parseIndexProp(prop, L"dc", snapshot.cascadesCount + 1);
	if (cascadeIndex == -2) {
		return L"0";
	}
	if (cascadeIndex >= 0) {
		if (cascadeIndex > 0) {
			cascadeIndex--;
		}
		printer.print(snapshot.dc[cascadeIndex]);
		return true;
	}

	cascadeIndex = legacy_parseIndexProp(prop, L"resolution", snapshot.cascadesCount + 1);
	if (cascadeIndex == -2) {
		return L"0";
	}
	if (cascadeIndex < 0) {
		cascadeIndex = legacy_parseIndexProp(prop, L"binWidth", snapshot.cascadesCount + 1);
	}
	if (cascadeIndex >= 0) {
		if (cascadeIndex > 0) {
			cascadeIndex--;
		}
		const auto resolution = static_cast<double>(snapshot.sampleRate) / snapshot.fftSize / std::pow(2, cascadeIndex);
		printer.print(resolution);
		return true;
	}

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
