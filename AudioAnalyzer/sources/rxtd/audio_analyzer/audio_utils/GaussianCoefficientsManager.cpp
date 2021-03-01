/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "GaussianCoefficientsManager.h"

using rxtd::audio_analyzer::audio_utils::GaussianCoefficientsManager;

std::vector<double> GaussianCoefficientsManager::generateGaussianKernel(index radius) {
	std::vector<double> kernel;
	kernel.resize(static_cast<std::vector<double>::size_type>(radius * 2 + 1));

	const double restoredSigma = static_cast<double>(radius) * (1.0 / 3.0);
	const double powerFactor = 1.0 / (2.0 * restoredSigma * restoredSigma);

	double r = -static_cast<double>(radius);
	double sum = 0.0;
	for (double& k : kernel) {
		k = std::exp(-r * r * powerFactor);
		sum += k;
		r++;
	}
	const double sumInverse = 1.0 / sum;
	for (auto& c : kernel) {
		c *= sumInverse;
	}

	return kernel;
}
