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
#include "rxtd/option_parsing/OptionParser.h"

namespace rxtd::fft_utils {
	class WindowFunctionHelper {
		// http://en.wikipedia.org/wiki/Window_function

	public:
		using WindowCreationFunc = std::function<void(array_span<float> result)>;

		static WindowCreationFunc parse(sview desc, option_parsing::OptionParser parser, Logger& cl);

		/// <summary>
		/// Create simple rectangular window.
		/// Same as using no window at all
		/// </summary>
		static void createRectangular(array_span<float> result);

		/// <summary>
		/// Create window from cosine-sum family, also known as Generalized Cosine Windows.
		/// This implementation only allows for the case K=1
		/// See: https://en.wikipedia.org/wiki/Window_function#Cosine-sum_windows
		/// </summary>
		static void createCosineSum(array_span<float> result, float a0);

		/// <summary>
		/// Create Kaiser window.
		/// See: https://en.wikipedia.org/wiki/Window_function#Kaiser_window
		/// </summary>
		static void createKaiser(array_span<float> result, float alpha);

		/// <summary>
		/// Create exponential window
		/// See: https://en.wikipedia.org/wiki/Window_function#Exponential_or_Poisson_window
		/// </summary>
		static void createExponential(array_span<float> result, float targetDecay);

		/// <summary>
		/// Create Dolph–Chebyshev window
		/// See: https://en.wikipedia.org/wiki/Window_function#Dolph–Chebyshev_window
		/// </summary>
		/// <param name="result"></param>
		/// <param name="attenuation"></param>
		static void createChebyshev(array_span<float> result, float attenuation);
	};
}
