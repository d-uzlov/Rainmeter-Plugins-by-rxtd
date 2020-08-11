/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <functional>
#include "RainmeterWrappers.h"

namespace rxtd::audio_utils {
	class WindowFunctionHelper {
		// http://en.wikipedia.org/wiki/Window_function

	public:
		using WindowCreationFunc = std::function<std::vector<float>(index size)>;

		static WindowCreationFunc parse(sview desc, utils::Rainmeter::Logger& cl);

		[[nodiscard]]
		static std::vector<float> createRectangular(index size);

		[[nodiscard]]
		static std::vector<float> createCosineSum(index size, double a0);

		[[nodiscard]]
		static std::vector<float> createKaiser(index size, double alpha);

		[[nodiscard]]
		static std::vector<float> createExponential(index size, double targetDecay);

		[[nodiscard]]
		static std::vector<float> createChebyshev(index size, double attenuation);
	};
}
