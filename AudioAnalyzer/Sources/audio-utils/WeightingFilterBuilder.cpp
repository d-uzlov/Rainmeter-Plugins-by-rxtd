/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WeightingFilterBuilder.h"

using namespace audio_utils;

BiQuadIIR WeightingFilterBuilder::createHighShelf(double samplingFrequency) {
	if (samplingFrequency == 0.0) {
		return { };
	}

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

	return {
		1.0 + K / Q + K * K,
		2.0 * (K * K - 1.0),
		(1.0 - K / Q + K * K),
		(Vh + Vb * K / Q + K * K),
		2.0 * (K * K - Vh),
		(Vh - Vb * K / Q + K * K)
	};
}

BiQuadIIR WeightingFilterBuilder::createHighPass(double samplingFrequency) {
	const static double pi = std::acos(-1.0);

	const double fc = 38.13547087613982;
	const double Q = 0.5003270373253953;
	const double K = std::tan(pi * fc / samplingFrequency);

	return {
		1.0,
		2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K),
		(1.0 - K / Q + K * K) / (1.0 + K / Q + K * K),
		1.0,
		-2.0,
		1.0
	};
}
