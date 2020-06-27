/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Loudness.h"

#include "undef.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;


std::optional<Loudness::Params> Loudness::parseParams(
	const utils::OptionMap& optionMap, utils::Rainmeter::Logger& cl) {
	Params params;
	params.attackTime = std::max(optionMap.get(L"attack").asFloat(100), 0.0) * 0.001;
	params.decayTime = std::max(optionMap.get(L"decay"sv).asFloat(params.attackTime), 0.0) * 0.001;

	return params;
}

void Loudness::updateFilter(index blockSize) {
	if (blockSize == this->blockSize) {
		return;
	}
	this->blockSize = blockSize;

	filter.setParams(params.attackTime, params.decayTime, samplesPerSec, blockSize);
}

audio_utils::InfiniteResponseFilter KWeightingFilterBuilder::createHighShelf(double samplingFrequency) {
	if (samplingFrequency == 0.0) {
		return { };
	}

	const static double pi = std::acos(-1.0);

	// V are gain values
	// Q is a "magic number" that effects the shape of the filter
	// Fc is the nominal cutoff frequency.
	//    That is, it's the cutoff frequency as a percentage of the sampling rate, 
	//    and "pre-warped" with tan() to match the frequency warping done by the bilinear transform

	const double fc = 1681.9744509555319;
	const double G = 3.99984385397;
	const double Q = 0.7071752369554193;
	const double K = std::tan(pi * fc / samplingFrequency);

	const double Vh = std::pow(10.0, G / 20.0);
	const double Vb = std::pow(Vh, 0.499666774155);
	const double a0_ = 1.0 + K / Q + K * K;

	std::vector<double> b = {
		(Vh + Vb * K / Q + K * K) / a0_,
		2.0 * (K * K - Vh) / a0_,
		(Vh - Vb * K / Q + K * K) / a0_
	};

	std::vector<double> a = {
		1.0,
		2.0 * (K * K - 1.0) / a0_,
		(1.0 - K / Q + K * K) / a0_
	};

	return { std::move(a), std::move(b) };
}

audio_utils::InfiniteResponseFilter KWeightingFilterBuilder::createHighPass(double samplingFrequency) {
	const static double pi = std::acos(-1.0);

	const double fc = 38.13547087613982;
	const double Q = 0.5003270373253953;
	const double K = std::tan(pi * fc / samplingFrequency);

	std::vector<double> a = {
		1.0,
		2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K),
		(1.0 - K / Q + K * K) / (1.0 + K / Q + K * K)
	};
	std::vector<double> b = {
		1.0,
		-2.0,
		1.0
	};

	return { std::move(a), std::move(b) };
}


void Loudness::setParams(Params params) {
	this->params = params;
	blockSize = 0; // this must cause filter to update for new attack/decay
}

void Loudness::setSamplesPerSec(index samplesPerSec) {
	this->samplesPerSec = samplesPerSec;
	blockSize = 0; // this must cause filter to update for new attack/decay

	highShelfFilter = KWeightingFilterBuilder::createHighShelf(samplesPerSec);
	highPassFilter = KWeightingFilterBuilder::createHighPass(samplesPerSec);
}

void Loudness::reset() {
	result = 0.0;
	filter.reset();
}

void Loudness::process(const DataSupplier& dataSupplier) {
	auto wave = dataSupplier.getWave();
	updateFilter(wave.size());
	intermediateWave.resize(wave.size());
	std::copy(wave.begin(), wave.end(), intermediateWave.begin());
	preprocessWave();
	const double loudness = calculateLoudness();
	const double lufs = std::max(loudness, -70.0) * (1.0 / 70.0) + 1;
	result = filter.next(lufs);
}

void Loudness::processSilence(const DataSupplier& dataSupplier) {
	auto wave = dataSupplier.getWave();
	updateFilter(wave.size());
	result = filter.next(0.0);
}

const wchar_t* Loudness::getProp(const isview& prop) const {
	if (prop == L"attack") {
		propString = std::to_wstring(params.attackTime * 1000.0);
	} else if (prop == L"decay") {
		propString = std::to_wstring(params.decayTime * 1000.0);
	} else {
		return nullptr;
	}
	return propString.c_str();
}

void Loudness::preprocessWave() {
	highShelfFilter.apply(intermediateWave);
	highPassFilter.apply(intermediateWave);
}

double Loudness::calculateLoudness() {
	double rms = 0.0;
	for (auto value : intermediateWave) {
		rms += value * value;
	}
	rms /= intermediateWave.size();

	const double loudness = -0.691 + 10.0 * std::log10(rms);

	return loudness;
}
