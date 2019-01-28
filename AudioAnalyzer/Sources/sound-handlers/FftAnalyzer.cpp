/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FftAnalyzer.h"

#include "undef.h"

using namespace std::literals::string_view_literals;

void rxaa::FftAnalyzer::CascadeData::setParams(FftAnalyzer* parent, CascadeData *successor, index cascadeIndex) {
	this->parent = parent;
	this->successor = successor;

	filledElements = 0u;
	transferredElements = 0u;
	ringBuffer.resize(parent->fftSize);
	values.resize(parent->fftSize / 2);
	std::fill(values.begin(), values.end(), 0.0);

	auto samplesPerSec = parent->samplesPerSec;
	samplesPerSec /= std::pow(2, cascadeIndex);

	attackDecay[0] = calculateAttackDecayConstant(parent->params.attackTime, samplesPerSec, parent->inputStride);
	attackDecay[1] = calculateAttackDecayConstant(parent->params.decayTime, samplesPerSec, parent->inputStride);

	downsampleGain = std::pow(2, cascadeIndex * 0.5);
}

void rxaa::FftAnalyzer::CascadeData::process(const float* wave, index waveSize) {
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

void rxaa::FftAnalyzer::CascadeData::processRandom(index waveSize, double amplitude) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;
	auto& random = parent->random;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != waveSize) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= waveSize) {
			for (index i = 0; i < copySize; ++i) {
				tmpIn[filledElements + i] = random.next() * amplitude;
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
				tmpIn[filledElements + i] = random.next();
			}

			filledElements = filledElements + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}
}

void rxaa::FftAnalyzer::CascadeData::processResampled(const float* const wave, index waveSize) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;

	if (waveSize <= 0u) {
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

void rxaa::FftAnalyzer::CascadeData::processSilent(index waveSize) {
	const auto fftSize = parent->fftSize;
	const auto inputStride = parent->inputStride;

	index waveProcessed = 0;
	const auto tmpIn = ringBuffer.data();

	while (waveProcessed != waveSize) {
		const auto copySize = fftSize - filledElements;

		if (waveProcessed + copySize <= waveSize) {
			std::fill_n(tmpIn + filledElements, copySize, 0.0);

			if (successor != nullptr) {
				successor->processResampled(tmpIn + transferredElements, ringBuffer.size() - transferredElements);
			}

			waveProcessed += copySize;

			doFft();
			std::copy(tmpIn + inputStride, tmpIn + fftSize, tmpIn);
			filledElements = fftSize - inputStride;
			transferredElements = filledElements;
		} else {
			std::fill_n(tmpIn + filledElements, waveSize - waveProcessed, 0.0);

			filledElements = filledElements + waveSize - waveProcessed;
			waveProcessed = waveSize;
		}
	}

}

void rxaa::FftAnalyzer::CascadeData::doFft() {
	parent->fftImpl->process(ringBuffer.data());

	const auto binsCount = parent->fftSize / 2;
	const auto fftImpl = parent->fftImpl;
	const auto correctZero = parent->params.correctZero;

	const auto newDC = fftImpl->getDC();
	dc = newDC + attackDecay[(newDC < dc)] * (dc - newDC);

	auto zerothBin = std::abs(newDC);
	if (correctZero) {
		constexpr double root2 = 1.4142135623730950488;
		zerothBin *= root2;
	}
	zerothBin = zerothBin * downsampleGain;

	values[0] = zerothBin + attackDecay[(zerothBin < values[0])] * (values[0] - zerothBin);

	for (index bin = 1; bin < binsCount; ++bin) {
		const double oldValue = values[bin];
		const double newValue = fftImpl->getBinMagnitude(bin) * downsampleGain;
		values[bin] = newValue + attackDecay[(newValue < oldValue)] * (oldValue - newValue);
	}
}

void rxaa::FftAnalyzer::CascadeData::reset() {
	std::fill(values.begin(), values.end(), 0.0);
}

rxaa::FftAnalyzer::Random::Random() : e2(rd()), dist(-1.0, 1.0) { }

double rxaa::FftAnalyzer::Random::next() {
	return dist(e2);
}

rxaa::FftAnalyzer::~FftAnalyzer() {
	fftImpl = FftImpl::change(fftImpl, 0);
}

std::optional<rxaa::FftAnalyzer::Params> rxaa::FftAnalyzer::parseParams(const utils::OptionParser::OptionMap &optionMap, utils::Rainmeter::ContextLogger& cl) {
	Params params;
	params.attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.1) * 0.001;
	params.decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.attackTime), 0.1) * 0.001;
	params.overlap = std::clamp(optionMap.get(L"overlap"sv).asFloat(0.5), 0.0, 1.0);

	params.cascadesCount = optionMap.get(L"cascadesCount"sv).asInt(5);
	if (params.cascadesCount <= 0) {
		cl.warning(L"cascadesCount must be >= 1 but {} found. Assume 1", params.cascadesCount);
		params.cascadesCount = 1;
	}

	params.randomTest = std::abs(optionMap.get(L"testRandom"sv).asFloat(0.0));
	params.correctZero = optionMap.get(L"correctZero"sv).asBool(true);

	const auto sizeBy = optionMap.get(L"sizeBy"sv).asIString(L"resolution");

	if (sizeBy == L"resolution") {
		params.resolution = optionMap.get(L"resolution"sv).asFloat(100.0);
		if (params.resolution <= 0.0) {
			cl.error(L"Resolution must be > 0 but {} found", params.resolution);
			return std::nullopt;
		}
		if (params.resolution <= 1.0) {
			cl.warning(L"Resolution {} is too small, use values > 1", params.resolution);
		}
		params.sizeBy = SizeBy::RESOLUTION;
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

	return params;
}

double rxaa::FftAnalyzer::getFftFreq(index fft) const {
	if (fft > fftSize / 2) {
		return 0.0;
	}
	return static_cast<double>(fft) * samplesPerSec / fftSize;
}

index rxaa::FftAnalyzer::getFftSize() const {
	return fftSize;
}

index rxaa::FftAnalyzer::getCascadesCount() const {
	return cascades.size();
}

const double* rxaa::FftAnalyzer::getCascade(index cascade) const { // TODO vector_view
	return cascades[cascade].values.data();
}

const double* rxaa::FftAnalyzer::getData() const {
	return getCascade(0u);
}

index rxaa::FftAnalyzer::getCount() const {
	return fftSize / 2;
}

void rxaa::FftAnalyzer::process(const DataSupplier& dataSupplier) {
	if (fftSize <= 0) {
		return;
	}

	const auto waveSize = dataSupplier.getWaveSize();

	const auto fftInputBuffer = dataSupplier.getBuffer<FftImpl::input_buffer_type>(fftImpl->getInputBufferSize());
	const auto fftOutputBuffer = dataSupplier.getBuffer<FftImpl::output_buffer_type>(fftImpl->getOutputBufferSize());
	fftImpl->setBuffers(fftInputBuffer, fftOutputBuffer);

	if (params.randomTest != 0.0) {
		cascades[0].processRandom(waveSize, params.randomTest);
	} else {
		const auto wave = dataSupplier.getWave();
		cascades[0].process(wave, waveSize);
	}

	// TODO that's ugly and error prone
	fftImpl->setBuffers(nullptr, nullptr);
}

void rxaa::FftAnalyzer::processSilence(const DataSupplier& dataSupplier) {
	if (fftSize <= 0) {
		return;
	}

	const auto fftInputBuffer = dataSupplier.getBuffer<FftImpl::input_buffer_type>(fftImpl->getInputBufferSize());
	const auto fftOutputBuffer = dataSupplier.getBuffer<FftImpl::output_buffer_type>(fftImpl->getOutputBufferSize());
	fftImpl->setBuffers(fftInputBuffer, fftOutputBuffer);

	const auto waveSize = dataSupplier.getWaveSize();

	if (params.randomTest != 0.0) {
		cascades[0].processRandom(waveSize, params.randomTest);
	} else {
		cascades[0].processSilent(waveSize);
	}

	// TODO that's ugly and error prone
	fftImpl->setBuffers(nullptr, nullptr);
}

void rxaa::FftAnalyzer::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
	updateParams();
}

const wchar_t* rxaa::FftAnalyzer::getProp(const isview& prop) const {
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

void rxaa::FftAnalyzer::reset() {
	for (auto& cascade : cascades) {
		cascade.reset();
	}
}

void rxaa::FftAnalyzer::setParams(Params params) {
	this->params = params;

	updateParams();
}

void rxaa::FftAnalyzer::updateParams() {
	switch (params.sizeBy) {
	case SizeBy::RESOLUTION:
		fftSize = kiss_fft::calculateNextFastSize(samplesPerSec / params.resolution, true);
		break;
	case SizeBy::SIZE:
		fftSize = kiss_fft::calculateNextFastSize(params.resolution, true);
		break;
	case SizeBy::SIZE_EXACT:
		fftSize = static_cast<index>(static_cast<size_t>(params.resolution) & ~1); // only even sizes are allowed
		break;
	default: // must be unreachable statement
		std::abort();
	}
	if (fftSize <= 1) {
		fftSize = 0; // TODO eliminate 0u, 1u, etc.
		return;
	}
	fftImpl = FftImpl::change(fftImpl, fftSize);

	inputStride = static_cast<index>(fftSize * (1 - params.overlap));
	inputStride = std::clamp<index>(inputStride, std::min<index>(16, fftSize), fftSize);

	cascades.resize(params.cascadesCount);
	for (index i = 0; i < index(cascades.size()); i++) {
		const auto next = i + 1 < cascades.size() ? &cascades[i + 1] : nullptr;
		cascades[i].setParams(this, next, i);
	}
}
