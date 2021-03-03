/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WindowFunctionHelper.h"

#include "libs/cheby_win/cheby_win.h"
#include "rxtd/option_parsing/OptionList.h"
#include "rxtd/option_parsing/OptionSequence.h"
#include "rxtd/std_fixes/MyMath.h"

using rxtd::fft_utils::WindowFunctionHelper;
using rxtd::option_parsing::Option;
using rxtd::option_parsing::OptionList;

WindowFunctionHelper::WindowCreationFunc WindowFunctionHelper::parse(sview desc, option_parsing::OptionParser parser, Logger& cl) {
	OptionList description = Option{ desc }.asSequence().begin().operator*();
	auto type = description.get(0).asIString();

	if (type == L"none") {
		return [](array_span<float> result) {
			createRectangular(result);
		};
	}

	if (type == L"hann") {
		return [](array_span<float> result) {
			createCosineSum(result, 0.5f);
		};
	}

	if (type == L"hamming") {
		return [](array_span<float> result) {
			createCosineSum(result, 0.53836f);
		};
	}

	if (type == L"kaiser") {
		const auto param = parser.parseFloatF(description.get(1), 3.0f);
		return [=](array_span<float> result) {
			createKaiser(result, param);
		};
	}

	if (type == L"exponential") {
		const auto param = parser.parseFloatF(description.get(1), 8.69f);
		return [=](array_span<float> result) {
			return createExponential(result, param);
		};
	}

	if (type == L"chebyshev") {
		const auto param = parser.parseFloatF(description.get(1), 80.0f);
		return [=](array_span<float> result) {
			return createChebyshev(result, param);
		};
	}

	cl.error(L"unknown windows function '{}', use none instead", type);
	return [](array_span<float> result) {
		return createRectangular(result);
	};
}

//
// Windows are asymmetric, so I use "i / size" everywhere instead of "* i / (size - 1)"
//

void WindowFunctionHelper::createRectangular(array_span<float> result) {
	for (auto& val : result) {
		val = 1.0f;
	}
}

void WindowFunctionHelper::createCosineSum(array_span<float> result, float a0) {
	float reverseSize = 1.0f / static_cast<float>(result.size());
	for (index i = 0; i < result.size(); ++i) {
		result[i] = a0 - (1.0f - a0) * std::cos(2.0f * std_fixes::MyMath::pif * static_cast<float>(i) * reverseSize);
	}
}

void WindowFunctionHelper::createKaiser(array_span<float> result, float alpha) {
	const double pia = std_fixes::MyMath::pi * alpha;
	const double inverseDenominator = 1.0 / std::cyl_bessel_i(0.0, pia);

	const double size = static_cast<double>(result.size());
	for (index i = 0; i < result.size(); ++i) {
		const double squaredContent = 2.0 * static_cast<double>(i) / size - 1.0;
		const double rootContent = 1.0 - squaredContent * squaredContent;
		const double value = std::cyl_bessel_i(0.0, pia * std::sqrt(rootContent));
		result[i] = static_cast<float>(value * inverseDenominator);
	}
}

void WindowFunctionHelper::createExponential(array_span<float> result, float targetDecay) {
	const float tau = static_cast<float>(result.size()) * 0.5f * 8.69f / targetDecay;
	const float tauInverse = 1.0f / tau;

	const float size = static_cast<float>(result.size());
	for (index i = 0; i < result.size(); ++i) {
		result[i] = std::exp(-std::abs(static_cast<float>(i) - size * 0.5f) * tauInverse);
	}
}

void WindowFunctionHelper::createChebyshev(array_span<float> result, float attenuation) {
	cheby_win(result.data(), static_cast<int>(result.size()), attenuation);
}
