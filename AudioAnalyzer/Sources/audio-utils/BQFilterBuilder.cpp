/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BQFilterBuilder.h"
#include "Math.h"

using namespace audio_utils;

BiQuadIIR BQFilterBuilder::createKWHighShelf(double samplingFrequency) {
	const double fc = 1681.9744509555319;
	const double G = 3.99984385397;
	const double Q = 0.7071752369554193;

	return createHighShelf(G, Q, fc, samplingFrequency);
}

BiQuadIIR BQFilterBuilder::createKWHighPass(double samplingFrequency) {
	const double fc = 38.13547087613982;
	const double Q = 0.5003270373253953;

	return createHighPass(Q, fc, samplingFrequency);
}

BiQuadIIR BQFilterBuilder::createHighShelf(double dbGain, double q, double centralFrequency, double samplingFrequency) {
	if (samplingFrequency == 0.0 || q == 0) {
		return { };
	}

	const double a = std::pow(10, dbGain / 40);
	const double w0 = 2 * utils::Math::pi * centralFrequency / samplingFrequency;
	const double alpha = std::sin(w0) / (2 * q);

	return {
				  (a + 1) - (a - 1) * std::cos(w0) + 2 * std::sqrt(a) * alpha,
			 2 * ((a - 1) - (a + 1) * std::cos(w0)),
				  (a + 1) - (a - 1) * std::cos(w0) - 2 * std::sqrt(a) * alpha,
			 a * ((a + 1) + (a - 1) * std::cos(w0) + 2 * std::sqrt(a) * alpha),
		-2 * a * ((a - 1) + (a + 1) * std::cos(w0)),
			 a * ((a + 1) + (a - 1) * std::cos(w0) - 2 * std::sqrt(a) * alpha),
	};
}

BiQuadIIR BQFilterBuilder::createLowShelf(double dbGain, double q, double centralFrequency, double samplingFrequency) {
	auto result = createHighShelf(-dbGain, q, centralFrequency, samplingFrequency);
	result.addGain(dbGain);
	return result;
}

BiQuadIIR BQFilterBuilder::createHighPass(double q, double centralFrequency, double samplingFrequency) {
	if (samplingFrequency == 0.0 || q == 0) {
		return { };
	}

	const double w0 = 2 * utils::Math::pi * centralFrequency / samplingFrequency;
	const double alpha = std::sin(w0) / (2 * q);

	return {
		  1 + alpha,
		 -2 * std::cos(w0),
		  1 - alpha,
		 (1 + std::cos(w0)) / 2,
		-(1 + std::cos(w0)),
		 (1 + std::cos(w0)) / 2,
	};
}

BiQuadIIR BQFilterBuilder::createLowPass(double q, double centralFrequency, double samplingFrequency) {
	if (samplingFrequency == 0.0 || q == 0) {
		return { };
	}

	const double w0 = 2 * utils::Math::pi * centralFrequency / samplingFrequency;
	const double alpha = std::sin(w0) / (2 * q);

	return {
		  1 + alpha,
		 -2 * std::cos(w0),
		  1 - alpha,
		 (1 + std::cos(w0)) / 2,
		 (1 + std::cos(w0)),
		 (1 + std::cos(w0)) / 2,
	};
}
