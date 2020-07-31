/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ButterworthWrapper.h"
#include "iir.h"

using namespace audio_utils;

FilterParameters ButterworthWrapper::calcCoefLowPass(index _order, double digitalCutoff) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoff = std::clamp(digitalCutoff, 0.01, 0.5 - 0.01);

	double* aCoef = dcof_bwlp(order, digitalCutoff);
	int* bCoef = ccof_bwlp(order);
	const double scalingFactor = sf_bwlp(order, digitalCutoff);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}

FilterParameters ButterworthWrapper::calcCoefHighPass(index _order, double digitalCutoff) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoff = std::clamp(digitalCutoff, 0.01, 0.5 - 0.01);

	double* aCoef = dcof_bwhp(order, digitalCutoff);
	int* bCoef = ccof_bwhp(order);
	const double scalingFactor = sf_bwhp(order, digitalCutoff);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}

FilterParameters ButterworthWrapper::calcCoefBandPass(index _order, double digitalCutoffLow, double digitalCutoffHigh) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoffLow = std::clamp(digitalCutoffLow, 0.01, 0.5 - 0.01);
	digitalCutoffHigh = std::clamp(digitalCutoffHigh, 0.01, 0.5 - 0.01);

	double* aCoef = dcof_bwbp(order, digitalCutoffLow, digitalCutoffHigh);
	int* bCoef = ccof_bwbp(order);
	const double scalingFactor = sf_bwbp(order, digitalCutoffLow, digitalCutoffHigh);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}

FilterParameters ButterworthWrapper::calcCoefBandStop(index _order, double digitalCutoffLow, double digitalCutoffHigh) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoffLow = std::clamp(digitalCutoffLow, 0.01, 0.5 - 0.01);
	digitalCutoffHigh = std::clamp(digitalCutoffHigh, 0.01, 0.5 - 0.01);

	double* aCoef = dcof_bwbs(order, digitalCutoffLow, digitalCutoffHigh);
	double* bCoef = ccof_bwbs(order, digitalCutoffLow, digitalCutoffHigh);
	const double scalingFactor = sf_bwbs(order, digitalCutoffLow, digitalCutoffHigh);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}
