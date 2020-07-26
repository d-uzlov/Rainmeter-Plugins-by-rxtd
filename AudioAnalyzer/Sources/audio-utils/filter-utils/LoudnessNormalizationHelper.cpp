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
#include "ButterworthWrapper.h"

#include "undef.h"

using namespace audio_utils;

FilterCascade LoudnessNormalizationHelper::getInstance(double samplingFrequency) {
	std::vector<std::unique_ptr<AbstractFilter>> filters;

	auto filter1 = new BiQuadIIR{ };
	*filter1 = createFilter1(samplingFrequency);
	filters.push_back(std::unique_ptr<AbstractFilter>(filter1));

	auto filter2 = new BiQuadIIR{ };
	*filter2 = createFilter2(samplingFrequency);
	filters.push_back(std::unique_ptr<AbstractFilter>(filter2));

	auto filter3 = new BiQuadIIR{ };
	*filter3 = createFilter3(samplingFrequency);
	filters.push_back(std::unique_ptr<AbstractFilter>(filter3));

	auto filter4 = new InfiniteResponseFilterFixed<BWOrder + 1>{ };
	*filter4 = createFilter4(samplingFrequency);
	filters.push_back(std::unique_ptr<AbstractFilter>(filter4));

	return { std::move(filters) };
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

InfiniteResponseFilterFixed<LoudnessNormalizationHelper::BWOrder + 1>
LoudnessNormalizationHelper::createFilter4(double samplingFrequency) {
	return ButterworthWrapper::createLowPass<BWOrder>(20000, samplingFrequency);
}
