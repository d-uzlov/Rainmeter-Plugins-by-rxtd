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

using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void FftAnalyzer::CascadeData::setParams(FftAnalyzer* parent, CascadeData *successor, layer_t cascadeIndex) {
	this->parent = parent;
	this->successor = successor;

	filledElements = 0;
	transferredElements = 0;
	ringBuffer.resize(parent->fftSize);

	const index newValuesSize = parent->fftSize / 2;
	if (values.empty()) {
		values.resize(newValuesSize);
	} else if (index(values.size()) != newValuesSize) {
		double coef = double(values.size()) / newValuesSize;

		if (index(values.size()) < newValuesSize) {
			values.resize(newValuesSize);

			for (int i = newValuesSize - 1; i >= 1; i--) {
				index oldIndex = std::clamp<index>(i * coef, 0, values.size() - 1);
				values[i] = values[oldIndex];
			}
		}

		if (index(values.size()) > newValuesSize) {
			for (int i = 1; i < newValuesSize; i++) {
				index oldIndex = std::clamp<index>(i * coef, 0, values.size() - 1);
				values[i] = values[oldIndex];
			}
			
			values.resize(newValuesSize);
		}

		values[0] = 0.0;
		// std::fill(values.begin(), values.end(), 0.0f);
	}

	auto samplesPerSec = parent->samplesPerSec;
	samplesPerSec = index(samplesPerSec / std::pow(2, cascadeIndex));

	filter.setParams(parent->params.attackTime, parent->params.decayTime, samplesPerSec, parent->inputStride);

	downsampleGain = std::pow(2, cascadeIndex * 0.5);
}

void FftAnalyzer::CascadeData::process(const float* wave, index waveSize) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != waveSize) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= waveSize) {
			const auto copyStartPlace = wave + waveProcessed;
			std::copy(copyStartPlace, copyStartPlace + copySize, tmpIn + filledElements);

			if (successor != nullptr) {
				successor->processResampled(tmpIn + transferredElements, ringBuffer.size() - transferredElements);
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			std::copy(wave + waveProcessed, wave + waveSize, tmpIn + filledElements);

			filledElements = filledElements + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}
}

void FftAnalyzer::CascadeData::processRandom(index waveSize, double amplitude) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;
	auto& random = parent->random;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != waveSize) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= waveSize) {
			for (index i = 0; i < copySize; ++i) {
				tmpIn[filledElements + i] = float(random.next() * amplitude);
			}

			if (successor != nullptr) {
				successor->processResampled(tmpIn + transferredElements, ringBuffer.size() - transferredElements);
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			for (index i = 0; i < waveSize - waveProcessed; ++i) {
				tmpIn[filledElements + i] = float(random.next());
			}

			filledElements = filledElements + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}
}

void FftAnalyzer::CascadeData::processResampled(const float* const wave, index waveSize) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;

	if (waveSize <= 0) {
		return;
	}

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();
	if (std::abs(odd) <= 1.0f) {
		tmpIn[filledElements] = (odd + wave[0]) * 0.5f;
		odd = 10.0f;
		filledElements++;
		waveProcessed = 1;
	}

	while (waveProcessed != waveSize) {
		const auto elementPairsNeeded = fftSize - filledElements;
		const auto elementPairsLeft = (waveSize - waveProcessed) / 2;
		const auto copyStartPlace = wave + waveProcessed;

		if (elementPairsNeeded <= elementPairsLeft) {
			for (index i = 0; i < elementPairsNeeded; i++) {
				tmpIn[filledElements + i] = (copyStartPlace[i * 2] + copyStartPlace[i * 2 + 1]) * 0.5f;
			}

			if (successor != nullptr) {
				successor->processResampled(tmpIn + transferredElements, ringBuffer.size() - transferredElements);
			}

			waveProcessed += elementPairsNeeded * 2;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			for (index i = 0; i < elementPairsLeft; i++) {
				tmpIn[filledElements + i] = (copyStartPlace[i * 2] + copyStartPlace[i * 2 + 1]) * 0.5f;
			}

			filledElements = filledElements + elementPairsLeft;
			waveProcessed += elementPairsLeft * 2;

			if (waveProcessed != waveSize) {
				// we should have exactly one element left
				odd = wave[waveProcessed];
				waveProcessed = waveSize;
			} else {
				odd = 10.0f;
			}
		}
	}
}

void FftAnalyzer::CascadeData::processSilence(index waveSize) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != waveSize) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= waveSize) {
			std::fill_n(tmpIn + filledElements, copySize, 0.0f);

			if (successor != nullptr) {
				successor->processResampled(tmpIn + transferredElements, ringBuffer.size() - transferredElements);
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			std::fill_n(tmpIn + filledElements, waveSize - waveProcessed, 0.0f);

			filledElements = filledElements + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}

}

void FftAnalyzer::CascadeData::doFft() {
	parent->fft.process(ringBuffer.data());

	const auto binsCount = parent->fftSize / 2;
	const auto fftImpl = parent->fft;
	const auto correctZero = parent->params.correctZero;

	const auto newDC = fftImpl.getDC();
	dc = filter.apply(dc, newDC);

	auto zerothBin = std::abs(newDC);
	if (correctZero) {
		constexpr double root2 = 1.4142135623730950488;
		zerothBin *= root2;
	}
	zerothBin = zerothBin * downsampleGain;

	values[0] = filter.apply(values[0], zerothBin);

	for (index bin = 1; bin < binsCount; ++bin) {
		const double oldValue = values[bin];
		const double newValue = fftImpl.getBinMagnitude(bin) * downsampleGain;
		values[bin] = filter.apply(oldValue, newValue);
	}
}

void FftAnalyzer::CascadeData::reset() {
	std::fill(values.begin(), values.end(), 0.0f);
}

FftAnalyzer::Random::Random() : e2(rd()), dist(-1.0, 1.0) { }

double FftAnalyzer::Random::next() {
	return dist(e2);
}

std::optional<FftAnalyzer::Params> FftAnalyzer::parseParams(const utils::OptionMap &optionMap, utils::Rainmeter::Logger& cl) {
	Params params{ };
	params.attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0);
	params.decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.attackTime), 0.0);

	params.attackTime *= 0.001;
	params.decayTime *= 0.001;


	const auto sizeBy = optionMap.get(L"sizeBy"sv).asIString(L"binWidth");
	if (sizeBy == L"binWidth") {
		params.resolution = optionMap.get(L"binWidth"sv).asFloat(100.0);
		if (params.resolution <= 0.0) {
			cl.error(L"Resolution must be > 0 but {} found", params.resolution);
			return std::nullopt;
		}
		if (params.resolution <= 1.0) {
			cl.warning(L"BinWidth {} is dangerously small, use values > 1", params.resolution);
		}
		params.sizeBy = SizeBy::BIN_WIDTH;
	} else if (sizeBy == L"size") {
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

	params.overlap = std::clamp(optionMap.get(L"overlap"sv).asFloat(0.5), 0.0, 1.0);

	params.cascadesCount = optionMap.get(L"cascadesCount"sv).asInt<layer_t>(5);
	if (params.cascadesCount <= 0) {
		cl.warning(L"cascadesCount must be >= 1 but {} found. Assume 1", params.cascadesCount);
		params.cascadesCount = 1;
	}

	params.correctZero = optionMap.get(L"correctZero"sv).asBool(true);

	params.randomTest = std::abs(optionMap.get(L"testRandom"sv).asFloat(0.0));
	params.randomDuration = std::abs(optionMap.get(L"randomDuration"sv).asFloat(1000.0)) * 0.001;

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

void FftAnalyzer::process(const DataSupplier& dataSupplier) {
	if (fftSize <= 0) {
		return;
	}

	const auto wave = dataSupplier.getWave();

	auto fftInputBuffer = dataSupplier.getBuffer<audio_utils::FFT::input_buffer_type>(fft.getInputBufferSize());
	auto fftOutputBuffer = dataSupplier.getBuffer<audio_utils::FFT::output_buffer_type>(fft.getOutputBufferSize());
	fft.setBuffers(fftInputBuffer.data(), fftOutputBuffer.data());

	if (params.randomTest != 0.0) {
		processRandom(wave.size());
	} else {
		cascades[0].process(wave.data(), wave.size());
	}

	// TODO that's ugly and error prone
	fft.setBuffers(nullptr, nullptr);
}

void FftAnalyzer::processSilence(const DataSupplier& dataSupplier) {
	if (fftSize <= 0) {
		return;
	}

	const auto waveSize = dataSupplier.getWave().size();

	auto fftInputBuffer = dataSupplier.getBuffer<audio_utils::FFT::input_buffer_type>(fft.getInputBufferSize());
	auto fftOutputBuffer = dataSupplier.getBuffer<audio_utils::FFT::output_buffer_type>(fft.getOutputBufferSize());
	fft.setBuffers(fftInputBuffer.data(), fftOutputBuffer.data());

	if (params.randomTest != 0.0) {
		processRandom(waveSize);
	} else {
		cascades[0].processSilence(waveSize);
	}

	// TODO that's ugly and error prone
	fft.setBuffers(nullptr, nullptr);
}

SoundHandler::layer_t FftAnalyzer::getLayersCount() const {
	return layer_t(cascades.size());
}

array_view<float> FftAnalyzer::getData(layer_t layer) const {
	return cascades[layer].values;
}

void FftAnalyzer::processRandom(index waveSize) {
	index remainingWave = waveSize;
	while (remainingWave != 0) {
		const index toProcess = std::min(randomBlockSize - randomCurrentOffset, remainingWave);
		if (randomState == RandomState::ON) {
			cascades[0].processRandom(waveSize, params.randomTest);
		} else {
			cascades[0].processSilence(waveSize);
		}
		remainingWave -= toProcess;
		randomCurrentOffset += toProcess;
		if (randomCurrentOffset == randomBlockSize) {
			randomCurrentOffset = 0;
			if (randomState == RandomState::ON) {
				randomState = RandomState::OFF;
			} else {
				randomState = RandomState::ON;
			}
		}
	}
}

void FftAnalyzer::setSamplesPerSec(index samplesPerSec) {
	if (this->samplesPerSec == samplesPerSec) {
		return;
	}

	this->samplesPerSec = samplesPerSec;
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
			propString = std::to_wstring(cascades[cascadeIndex].dc);
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
	for (layer_t i = 0; i < layer_t(cascades.size()); i++) {
		const auto next = i + 1 < static_cast<index>(cascades.size()) ? &cascades[i + 1] : nullptr;
		cascades[i].setParams(this, next, i);
	}
}
