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
#include "rxtd/Logger.h"

namespace rxtd::fft_utils {
	class WindowFunctionHelper {
		// http://en.wikipedia.org/wiki/Window_function

	public:
		using WindowCreationFunc = std::function<void(array_span<float> result)>;

		static WindowCreationFunc parse(sview desc, Logger& cl);
		
		static void createRectangular(array_span<float> result);
		
		static void createCosineSum(array_span<float> result, float a0);
		
		static void createKaiser(array_span<float> result, float alpha);
		
		static void createExponential(array_span<float> result, float targetDecay);
		
		static void createChebyshev(array_span<float> result, float attenuation);
	};
}
