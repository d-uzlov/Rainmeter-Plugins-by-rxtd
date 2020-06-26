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
#include <numeric>

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;


std::optional<Loudness::Params> Loudness::parseParams(const utils::OptionMap& optionMap,
                                                      utils::Rainmeter::Logger& cl) {
	Params params;
	params.resamplerId = optionMap.get(L"source").asIString();
	return params;
}

audio_utils::InfiniteResponseFilter KWeightingFilterBuilder::create1(double samplingFrequency) {
	if (samplingFrequency == 0.0) {
		return { };
	}

	// https://github.com/BrechtDeMan/loudness.py/blob/master/loudness.py
	const static double pi = std::acos(-1.0);

	// https://hydrogenaud.io/index.php?topic=86116.25
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

audio_utils::InfiniteResponseFilter KWeightingFilterBuilder::create2(double samplingFrequency) {
	// https://github.com/BrechtDeMan/loudness.py/blob/master/loudness.py
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
}

void Loudness::setSamplesPerSec(index samplesPerSec) {
	// this->samplesPerSec = samplesPerSec;
	filter1 = KWeightingFilterBuilder::create1(samplesPerSec);
	filter2 = KWeightingFilterBuilder::create2(samplesPerSec);
}

void Loudness::reset() {
	result = 0.0;
}

void Loudness::process(const DataSupplier& dataSupplier) {
	if (!valid) {
		return;
	}

	valid = false;

	auto wave = dataSupplier.getWave();
	intermediateWave.resize(wave.size());
	std::copy(wave.begin(), wave.end(), intermediateWave.begin());
	preprocessWave();
	result = calculateLoudness();

	changed = true;

	valid = true;
}

void Loudness::processSilence(const DataSupplier& dataSupplier) {
	process(dataSupplier);
}

const wchar_t* Loudness::getProp(const isview& prop) const {
	return nullptr;
}

void Loudness::preprocessWave() {
	filter1.apply(intermediateWave);
	filter2.apply(intermediateWave);
}

double Loudness::calculateLoudness() {
	const double minThreshold = -70.0;

	double rms = 0.0;
	for (auto value : intermediateWave) {
		rms += value * value;
	}
	rms /= intermediateWave.size();

	double loudness = -0.691 + 10.0 * std::log10(rms);
	loudness = std::max(loudness, minThreshold);

	return loudness;
}
