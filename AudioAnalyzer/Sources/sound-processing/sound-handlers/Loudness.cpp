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
#include "../../audio-utils/KWeightingFilterBuilder.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void Loudness::setSamplesPerSec(index samplesPerSec) {
	BlockHandler::setSamplesPerSec(samplesPerSec);

	highShelfFilter = audio_utils::KWeightingFilterBuilder::createHighShelf(samplesPerSec);
	highPassFilter = audio_utils::KWeightingFilterBuilder::createHighPass(samplesPerSec);
}

void Loudness::_reset() {
	intermediateRmsResult = 0.0;
}

void Loudness::process(const DataSupplier& dataSupplier) {
	auto wave = dataSupplier.getWave();
	intermediateWave.resize(wave.size());
	std::copy(wave.begin(), wave.end(), intermediateWave.begin());
	preprocessWave();

	for (double x : intermediateWave) {
		intermediateRmsResult += x * x;
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}

	// const double loudness = calculateLoudness();
	// const double lufs = std::max(loudness, -70.0) * (1.0 / 70.0) + 1;
	// result = filter.next(lufs);
}

void Loudness::finishBlock() {
	const double value = std::sqrt(intermediateRmsResult / getBlockSize());
	getFilter().next(value);
	counter = 0;
	intermediateRmsResult = 0.0;
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
