/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "LoudnessNormalizationHelper.h"
#include "BQFilterBuilder.h"

#include "undef.h"

using namespace audio_utils;

LoudnessNormalizationHelper::LoudnessNormalizationHelper(double samplingFrequency) {
	filter1 = createFilter1(samplingFrequency);
	filter2 = createFilter2(samplingFrequency);
	filter3 = createFilter3(samplingFrequency);
}

void LoudnessNormalizationHelper::apply(array_view<float> wave) {
	processed.resize(wave.size());
	std::copy(wave.begin(), wave.end(), processed.begin());

	filter1.apply(processed);
	filter2.apply(processed);
	filter3.apply(processed);
}

BiQuadIIR LoudnessNormalizationHelper::createFilter1(double samplingFrequency) {
	const double freq = 310.0;
	const double q = 0.5;

	return BQFilterBuilder::createHighPass(q, freq, samplingFrequency);
}

BiQuadIIR LoudnessNormalizationHelper::createFilter2(double samplingFrequency) {
	const double freq = 1125;
	const double gain = -4.1;
	const double q = 4.0;

	return BQFilterBuilder::createPeak(gain, q, freq, samplingFrequency);
}

BiQuadIIR LoudnessNormalizationHelper::createFilter3(double samplingFrequency) {
	const double freq = 3665;
	const double gain = 5.5;
	const double q = 4.0;

	auto result = BQFilterBuilder::createPeak(gain, q, freq, samplingFrequency);
	result.addGain(-gain);
	return result;
}

BiQuadIIR LoudnessNormalizationHelper::createFilter4(double samplingFrequency) {
	// butterworth, order 5, cutoff 9200
}
