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
	const double fc = 1681.9744509555319;
	const double G = 3.99984385397;
	const double Q = 0.7071752369554193;

	return BQFilterBuilder::createHighShelf(G, Q, fc, samplingFrequency);
}

BiQuadIIR LoudnessNormalizationHelper::createFilter2(double samplingFrequency) {
	const double fc = 38.13547087613982;
	const double Q = 0.5003270373253953;

	return BQFilterBuilder::createHighPass(Q, fc, samplingFrequency);
}

BiQuadIIR LoudnessNormalizationHelper::createFilter3(double samplingFrequency) {
	const double fc = 10'000;
	const double Q = 2.0;

	return BQFilterBuilder::createLowPass(Q, fc, samplingFrequency);
}
