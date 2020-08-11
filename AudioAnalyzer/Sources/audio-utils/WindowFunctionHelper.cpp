/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WindowFunctionHelper.h"

#include "RainmeterWrappers.h"
#include "option-parser/OptionList.h"
#include "option-parser/OptionSequence.h"
#include "cheby_win-lib/cheby_win.h"

using namespace audio_utils;

static const auto pi = std::acos(-1.0);

WindowFunctionHelper::WindowCreationFunc WindowFunctionHelper::parse(sview desc, utils::Rainmeter::Logger& cl) {
	utils::OptionList description  = utils::Option{ desc }.asSequence().begin().operator*();
	auto type = description.get(0).asIString();

	if (type == L"none") {
		return [](index size) {
			return createRectangular(size);
		};
	}
	
	if (type == L"hann") {
		return [](index size) {
			return createCosineSum(size, 0.5);
		};
	}
	
	if (type == L"hamming") {
		return [](index size) {
			return createCosineSum(size, 0.53836);
		};
	}
	
	if (type == L"kaiser") {
		const auto param = description.get(1).asFloat(3.0);
		return [=](index size) {
			return createKaiser(size, param);
		};
	}
	
	if (type == L"exponential") {
		const auto param = description.get(1).asFloat(8.69);
		return [=](index size) {
			return createExponential(size, param);
		};
	}

	if (type == L"chebyshev") {
		const auto param = description.get(1).asFloat(80.0);
		return [=](index size) {
			return createChebyshev(size, param);
		};
	}

	cl.error(L"unknown windows function '{}', use none instead", type);
	return [](index size) {
		return createRectangular(size);
	};
}

//
// Windows are asymmetric, so I use "i / size" everywhere instead of "* i / (size - 1)"
//

std::vector<float> WindowFunctionHelper::createRectangular(index size) {
	std::vector<float> window;
	window.resize(size);

	for (index i = 0; i < size; ++i) {
		window[i] = 1.0f;
	}

	return window;
}

std::vector<float> WindowFunctionHelper::createCosineSum(index size, double a0) {
	std::vector<float> window;
	window.resize(size);

	for (index i = 0; i < size; ++i) {
		const double value = a0 - (1.0 - a0) * std::cos(2 * pi * i / size);
		window[i] = float(value);
	}

	return window;
}

std::vector<float> WindowFunctionHelper::createKaiser(index size, double alpha) {
	std::vector<float> window;
	window.resize(size);

	const double pia = pi * alpha;
	const double inverseDenominator = 1.0 / std::cyl_bessel_i(0.0, pia);
	
	for (index i = 0; i < size; ++i) {
		const double squaredContent = 2.0 * i / size - 1.0;
		const double rootContent = 1.0 - squaredContent * squaredContent;
		const double value = std::cyl_bessel_i(0.0, pia * std::sqrt(rootContent));
		window[i] = float(value * inverseDenominator);
	}

	return window;
}

std::vector<float> WindowFunctionHelper::createExponential(index size, double targetDecay) {
	std::vector<float> window;
	window.resize(size);

	const double tau = size * 0.5 * 8.69 / targetDecay;
	const double tauInverse = 1.0 / tau;
	
	for (index i = 0; i < size; ++i) {
		const double value = std::exp(-std::abs(i - size * 0.5) * tauInverse);
		window[i] = float(value);
	}

	return window;
}

std::vector<float> WindowFunctionHelper::createChebyshev(index size, double attenuation) {
	std::vector<float> window;
	window.resize(size);

	cheby_win(window.data(), int(size), float(attenuation));
	
	return window;
}
