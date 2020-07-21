/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FftAnalyzer.h"
#include "option-parser/OptionMap.h"

#include "undef.h"
#include "../../../audio-utils/KWeightingFilterBuilder.h"
#include "../../../audio-utils/RandomGenerator.h"

using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

std::optional<FftAnalyzer::Params> FftAnalyzer::parseParams(
	const utils::OptionMap& optionMap,
	utils::Rainmeter::Logger& cl
) {
	Params params{ };
	params.attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0);
	params.decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.attackTime), 0.0);

	params.attackTime *= 0.001;
	params.decayTime *= 0.001;


	if (const auto sizeBy = optionMap.get(L"sizeBy"sv).asIString(L"binWidth");
		sizeBy == L"binWidth") {
		params.resolution = optionMap.get(L"binWidth"sv).asFloat(100.0);
		if (params.resolution <= 0.0) {
			cl.error(L"Resolution must be > 0 but {} found", params.resolution);
			return std::nullopt;
		}
		if (params.resolution <= 1.0) {
			cl.warning(L"BinWidth {} is dangerously small, use values > 1", params.resolution);
		}
		params.sizeBy = SizeBy::BIN_WIDTH;
	} else {
		cl.warning(L"Options 'sizeBy' is deprecated");

		if (sizeBy == L"size") {
			params.resolution = optionMap.get(L"size"sv).asInt(1000);
			if (params.resolution < 2) {
				cl.warning(L"Size must be >= 2 but {} found. Assume 1000", params.resolution);
				params.resolution = 1000;
			}
			params.sizeBy = SizeBy::SIZE;

		} else if (sizeBy == L"sizeExact") {
			params.resolution = optionMap.get(L"size"sv).asInt(1000);
			if (params.resolution < 2) {
				cl.error(L"Size must be >= 2, must be even, but {} found", params.resolution);
				return std::nullopt;
			}
			params.sizeBy = SizeBy::SIZE_EXACT;
		} else {
			cl.error(L"Unknown fft sizeBy '{}'", sizeBy);
			return std::nullopt;
		}
	}

	params.overlap = std::clamp(optionMap.get(L"overlap"sv).asFloat(0.5), 0.0, 1.0);

	params.cascadesCount = optionMap.get(L"cascadesCount"sv).asInt<layer_t>(5);
	if (params.cascadesCount <= 0) {
		cl.warning(L"cascadesCount must be >= 1 but {} found. Assume 1", params.cascadesCount);
		params.cascadesCount = 1;
	}

	params.correctLoudness = optionMap.get(L"correctLoudness"sv).asBool(false);

	params.randomTest = std::abs(optionMap.get(L"testRandom"sv).asFloat(0.0));
	params.randomDuration = std::abs(optionMap.get(L"randomDuration"sv).asFloat(1000.0)) * 0.001;

	params.correctZero = optionMap.get(L"correctZero"sv).asBool(true);

	return params;
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
	if (fftSize <= 0) {
		return;
	}

	const auto wave = dataSupplier.getWave();

	fft.setBuffers(
		dataSupplier.getBuffer<audio_utils::FFT::input_buffer_type>(fft.getInputBufferSize()),
		dataSupplier.getBuffer<audio_utils::FFT::output_buffer_type>(fft.getOutputBufferSize())
	);

	if (params.randomTest != 0.0) {
		processRandom(wave.size());
	} else if (params.correctLoudness) {
		waveBuffer.resize(wave.size());
		std::copy(wave.begin(), wave.end(), waveBuffer.begin());
		preprocessWave(waveBuffer);
		cascades[0].process(waveBuffer);
	} else {
		cascades[0].process(wave);
	}

	fft.resetBuffers();
}

void FftAnalyzer::_processSilence(const DataSupplier& dataSupplier) {
	if (fftSize <= 0) {
		return;
	}

	const auto waveSize = dataSupplier.getWave().size();

	fft.setBuffers(
		dataSupplier.getBuffer<audio_utils::FFT::input_buffer_type>(fft.getInputBufferSize()),
		dataSupplier.getBuffer<audio_utils::FFT::output_buffer_type>(fft.getOutputBufferSize())
	);

	if (params.randomTest != 0.0) {
		processRandom(waveSize);
	} else {
		cascades[0].processSilence(waveSize);
	}

	fft.resetBuffers();
}

SoundHandler::layer_t FftAnalyzer::getLayersCount() const {
	return layer_t(cascades.size());
}

array_view<float> FftAnalyzer::getData(layer_t layer) const {
	return cascades[layer].getValues();
}

void FftAnalyzer::processRandom(index waveSize) {
	audio_utils::RandomGenerator random;

	std::vector<float> wave;
	wave.resize(waveSize);

	for (index i = 0; i < waveSize; ++i) {
		if (randomState == RandomState::ON) {
			wave.push_back(random.next() * params.randomTest);
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

	if (params.correctLoudness) {
		preprocessWave(wave);
	}
	cascades[0].process(wave);
}

void FftAnalyzer::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;

	highShelfFilter = audio_utils::KWeightingFilterBuilder::createHighShelf(samplesPerSec);
	highPassFilter = audio_utils::KWeightingFilterBuilder::createHighPass(samplesPerSec);

	updateParams();
}

const wchar_t* FftAnalyzer::getProp(const isview& prop) const {
	propString.clear();

	if (prop == L"size") {
		propString = std::to_wstring(fftSize);
	} else if (prop == L"attack") {
		propString = std::to_wstring(params.attackTime * 1000.0);
	} else if (prop == L"decay") {
		propString = std::to_wstring(params.decayTime * 1000.0);
	} else if (prop == L"cascades count") {
		propString = std::to_wstring(cascades.size());
	} else if (prop == L"overlap") {
		propString = std::to_wstring(params.overlap);
	} else {
		auto cascadeIndex = parseIndexProp(prop, L"nyquist frequency", cascades.size() + 1);
		if (cascadeIndex == -2) {
			return L"0";
		}
		if (cascadeIndex >= 0) {
			if (cascadeIndex > 0) {
				cascadeIndex--;
			}
			propString = std::to_wstring(static_cast<index>(samplesPerSec / 2 / std::pow(2, cascadeIndex)));
			return propString.c_str();
		}

		cascadeIndex = parseIndexProp(prop, L"dc", cascades.size() + 1);
		if (cascadeIndex == -2) {
			return L"0";
		}
		if (cascadeIndex >= 0) {
			if (cascadeIndex > 0) {
				cascadeIndex--;
			}
			propString = std::to_wstring(cascades[cascadeIndex].getDC());
			return propString.c_str();
		}

		cascadeIndex = parseIndexProp(prop, L"resolution", cascades.size() + 1);
		if (cascadeIndex == -2) {
			return L"0";
		}
		if (cascadeIndex >= 0) {
			if (cascadeIndex > 0) {
				cascadeIndex--;
			}
			const auto resolution = static_cast<double>(samplesPerSec) / fftSize / std::pow(2, cascadeIndex);
			propString = std::to_wstring(resolution);
			return propString.c_str();
		}

		return nullptr;
	}

	return propString.c_str();
}

void FftAnalyzer::reset() {
	for (auto& cascade : cascades) {
		cascade.reset();
	}
}

void FftAnalyzer::setParams(Params params, Channel channel) {
	if (this->params == params) {
		return;
	}

	this->params = params;

	updateParams();
}

void FftAnalyzer::preprocessWave(array_span<float> wave) {
	highShelfFilter.apply(wave);
	highPassFilter.apply(wave);
}

void FftAnalyzer::updateParams() {
	switch (params.sizeBy) {
	case SizeBy::BIN_WIDTH:
		fftSize = kiss_fft::calculateNextFastSize(index(samplesPerSec / params.resolution), true);
		break;
	case SizeBy::SIZE:
		fftSize = kiss_fft::calculateNextFastSize(index(params.resolution), true);
		break;
	case SizeBy::SIZE_EXACT:
		fftSize = static_cast<index>(static_cast<size_t>(params.resolution) & ~1); // only even sizes are allowed
		break;
	default: // must be unreachable statement
		std::abort();
	}
	if (fftSize <= 1) {
		fftSize = 0;
		return;
	}
	fft.setSize(fftSize);

	inputStride = static_cast<index>(fftSize * (1 - params.overlap));
	inputStride = std::clamp<index>(inputStride, std::min<index>(16, fftSize), fftSize);

	randomBlockSize = index(params.randomDuration * samplesPerSec * fftSize / inputStride);
	randomCurrentOffset = 0;

	cascades.resize(params.cascadesCount);

	audio_utils::FftCascade::Params cascadeParams{ };
	cascadeParams.fftSize = fftSize;
	cascadeParams.samplesPerSec = samplesPerSec;
	cascadeParams.attackTime = params.attackTime;
	cascadeParams.decayTime = params.decayTime;
	cascadeParams.inputStride = inputStride;
	cascadeParams.correctZero = params.correctZero;

	for (layer_t i = 0; i < layer_t(cascades.size()); i++) {
		const auto next = i + 1 < static_cast<index>(cascades.size()) ? &cascades[i + 1] : nullptr;
		cascades[i].setParams(cascadeParams, &fft, next, i);
	}
}
