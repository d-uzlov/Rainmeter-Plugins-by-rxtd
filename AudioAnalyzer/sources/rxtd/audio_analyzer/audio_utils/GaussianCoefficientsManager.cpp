// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
