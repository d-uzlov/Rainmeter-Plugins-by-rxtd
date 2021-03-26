// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "GaussianCoefficientsManager.h"

#include "rxtd/std_fixes/MyMath.h"

using rxtd::audio_analyzer::audio_utils::GaussianCoefficientsManager;

std::vector<float> GaussianCoefficientsManager::createGaussianKernel(float radius) {
	std::vector<float> kernel;
	kernel.resize(std_fixes::MyMath::roundTo<size_t>(radius) * 2 + 1);
	if (kernel.empty()) {
		return kernel;
	}

	const float sigma = radius * (1.0f / 3.0f);
	const float powerFactor = 1.0f / (2.0f * sigma * sigma);

	float r = -radius;
	float sum = 0.0;
	for (auto& k : kernel) {
		k = std::exp(-r * r * powerFactor);
		sum += k;
		r += 1.0f;
	}
	const float sumInverse = 1.0f / sum;
	for (auto& c : kernel) {
		c *= sumInverse;
	}

	return kernel;
}
