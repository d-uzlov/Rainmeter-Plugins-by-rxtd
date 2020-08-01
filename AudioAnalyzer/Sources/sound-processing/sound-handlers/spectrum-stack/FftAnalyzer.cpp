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
#include "option-parser/OptionMap.h"

using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<FftAnalyzer::Params> FftAnalyzer::parseParams(const OptionMap& optionMap, Logger& cl) {
	Params params{ };

	params.legacy_attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0);
	params.legacy_decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.legacy_attackTime), 0.0);

	if (params.legacy_attackTime != 0 || params.legacy_decayTime != 0) {
		cl.warning(L"attack/decay options on FftAnalyzer are deprecated, use SingleValueTransformer instead");
	}

	params.legacy_attackTime *= 0.001;
	params.legacy_decayTime *= 0.001;

	if (const auto sizeBy = optionMap.get(L"sizeBy"sv).asIString(L"binWidth");
		sizeBy == L"binWidth") {
		params.binWidth = optionMap.get(L"binWidth"sv).asFloat(100.0);
		if (params.binWidth <= 0.0) {
			cl.error(L"Resolution must be > 0 but {} found", params.binWidth);
			return std::nullopt;
		}
		if (params.binWidth <= 1.0) {
			cl.warning(L"BinWidth {} is dangerously small, use values > 1", params.binWidth);
		}
		params.legacy_sizeBy = SizeBy::BIN_WIDTH;
	} else {
		cl.warning(L"Options 'sizeBy' is deprecated");

		if (sizeBy == L"size") {
			params.binWidth = optionMap.get(L"size"sv).asInt(1000);
			if (params.binWidth < 2) {
				cl.warning(L"Size must be >= 2 but {} found. Assume 1000", params.binWidth);
				params.binWidth = 1000;
			}
			params.legacy_sizeBy = SizeBy::SIZE;

		} else if (sizeBy == L"sizeExact") {
			params.binWidth = optionMap.get(L"size"sv).asInt(1000);
			if (params.binWidth < 2) {
				cl.error(L"Size must be >= 2, must be even, but {} found", params.binWidth);
				return std::nullopt;
			}
			params.legacy_sizeBy = SizeBy::SIZE_EXACT;
		} else {
			cl.error(L"Unknown fft sizeBy '{}'", sizeBy);
			return std::nullopt;
		}
	}

	params.overlap = std::clamp(optionMap.get(L"overlap"sv).asFloat(0.5), 0.0, 1.0);

	params.cascadesCount = optionMap.get(L"cascadesCount"sv).asInt(5);
	if (params.cascadesCount <= 0) {
		cl.warning(L"cascadesCount must be in range [1, 20] but {} found. Assume 1", params.cascadesCount);
		params.cascadesCount = 1;
	} else if (params.cascadesCount > 20) {
		cl.warning(L"cascadesCount must be in range [1, 20] but {} found. Assume 20", params.cascadesCount);
		params.cascadesCount = 20;
	}

	params.randomTest = std::abs(optionMap.get(L"testRandom"sv).asFloat(0.0));
	params.randomDuration = std::abs(optionMap.get(L"randomDuration"sv).asFloat(1000.0)) * 0.001;

	params.legacy_correctZero = optionMap.get(L"correctZero"sv).asBool(true);
	params.legacyAmplification = optionMap.get(L"legacyAmplification"sv).asBool(true);

	return params;
}

void FftAnalyzer::setParams(Params params, Channel channel) {
	if (this->params == params) {
		return;
	}

	this->params = params;

	updateParams();
}

double FftAnalyzer::getFftFreq(index fft) const {
	if (fft > fftSize / 2) {
		return 0.0;
	}
	return static_cast<double>(fft) * samplesPerSec / fftSize;
}

index FftAnalyzer::getFftSize() const {
	return fftSize;
}

void FftAnalyzer::_process(const DataSupplier& dataSupplier) {
	if (fftSize <= 0) { // effectively checks that sample rate is not 0
		return;
	}

	const auto wave = dataSupplier.getWave();

	if (params.randomTest != 0.0) {
		processRandom(wave.size());
	} else {
		cascades[0].process(wave);
	}

	for (index i = 0; i < index(cascades.size()); i++) {
		layers[i] = cascades[i].getLayerData();
	}
}

void FftAnalyzer::processRandom(index waveSize) {
	audio_utils::RandomGenerator random;

	std::vector<float> wave;
	wave.resize(waveSize);

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

	cascades[0].process(wave);
}

void FftAnalyzer::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	updateParams();
}

bool FftAnalyzer::getProp(const isview& prop, utils::BufferPrinter& printer) const {
	if (prop == L"size") {
		printer.print(fftSize);
	} else if (prop == L"attack") {
		printer.print(params.legacy_attackTime * 1000.0);
	} else if (prop == L"decay") {
		printer.print(params.legacy_decayTime * 1000.0);
	} else if (prop == L"cascades count") {
		printer.print(cascades.size());
	} else if (prop == L"overlap") {
		printer.print(params.overlap);
	} else {
		auto cascadeIndex = legacy_parseIndexProp(prop, L"nyquist frequency", cascades.size() + 1);
		if (cascadeIndex == -2) {
			return L"0";
		}
		if (cascadeIndex >= 0) {
			if (cascadeIndex > 0) {
				cascadeIndex--;
			}
			printer.print(static_cast<index>(samplesPerSec / 2.0 / std::pow(2, cascadeIndex)));
			return true;
		}

		cascadeIndex = legacy_parseIndexProp(prop, L"dc", cascades.size() + 1);
		if (cascadeIndex == -2) {
			return L"0";
		}
		if (cascadeIndex >= 0) {
			if (cascadeIndex > 0) {
				cascadeIndex--;
			}
			printer.print(cascades[cascadeIndex].legacy_getDC());
			return true;
		}

		cascadeIndex = legacy_parseIndexProp(prop, L"resolution", cascades.size() + 1);
		if (cascadeIndex == -2) {
			return L"0";
		}
		if (cascadeIndex < 0) {
			cascadeIndex = legacy_parseIndexProp(prop, L"binWidth", cascades.size() + 1);
		}
		if (cascadeIndex >= 0) {
			if (cascadeIndex > 0) {
				cascadeIndex--;
			}
			const auto resolution = static_cast<double>(samplesPerSec) / fftSize / std::pow(2, cascadeIndex);
			printer.print(resolution);
			return true;
		}

		return false;
	}

	return true;
}

void FftAnalyzer::reset() {
	for (auto& cascade : cascades) {
		cascade.reset();
	}
}

void FftAnalyzer::updateParams() {
	switch (params.legacy_sizeBy) {
	case SizeBy::BIN_WIDTH:
		fftSize = kiss_fft::calculateNextFastSize(index(samplesPerSec / params.binWidth), true);
		break;
	case SizeBy::SIZE:
		fftSize = kiss_fft::calculateNextFastSize(index(params.binWidth), true);
		break;
	case SizeBy::SIZE_EXACT:
		fftSize = static_cast<index>(static_cast<size_t>(params.binWidth) & ~1); // only even sizes are allowed
		break;
	default: // must be unreachable statement
		std::abort();
	}
	if (fftSize <= 1) {
		fftSize = 0;
		return;
	}
	fft.setSize(fftSize, !params.legacyAmplification);

	inputStride = static_cast<index>(fftSize * (1 - params.overlap));
	inputStride = std::clamp<index>(inputStride, std::min<index>(16, fftSize), fftSize);

	randomBlockSize = index(params.randomDuration * samplesPerSec * fftSize / inputStride);
	randomCurrentOffset = 0;

	cascades.resize(params.cascadesCount);
	layers.resize(params.cascadesCount);

	audio_utils::FftCascade::Params cascadeParams{ };
	cascadeParams.fftSize = fftSize;
	cascadeParams.samplesPerSec = samplesPerSec;
	cascadeParams.legacy_attackTime = params.legacy_attackTime;
	cascadeParams.legacy_decayTime = params.legacy_decayTime;
	cascadeParams.inputStride = inputStride;
	cascadeParams.legacy_correctZero = params.legacy_correctZero;

	for (index i = 0; i < index(cascades.size()); i++) {
		const auto next = i + 1 < index(cascades.size()) ? &cascades[i + 1] : nullptr;
		cascades[i].setParams(cascadeParams, &fft, next, i);
	}
}
