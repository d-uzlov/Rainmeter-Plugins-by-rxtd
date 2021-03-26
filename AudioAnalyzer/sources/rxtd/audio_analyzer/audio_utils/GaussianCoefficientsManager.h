// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

namespace rxtd::audio_analyzer::audio_utils {
	class GaussianCoefficientsManager {
	public:
		static std::vector<float> createGaussianKernel(float radius);
	};
}
