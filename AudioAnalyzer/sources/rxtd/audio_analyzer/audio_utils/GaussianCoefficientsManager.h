// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

namespace rxtd::audio_analyzer::audio_utils {
	class GaussianCoefficientsManager {
		// radius -> coefs vector
		std::map<index, std::vector<double>> blurCoefficients;

	public:
		array_view<double> forRadius(index radius) {
			auto& vec = blurCoefficients[radius];
			if (vec.empty()) {
				vec = generateGaussianKernel(radius);
			}

			return vec;
		}

	private:
		static std::vector<double> generateGaussianKernel(index radius);
	};
}
