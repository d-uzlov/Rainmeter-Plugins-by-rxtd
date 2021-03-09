// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "WindowFunctionHelper.h"

#include "ComplexFft.h"
#include "rxtd/std_fixes/MyMath.h"
#include "rxtd/option_parsing/Option.h"

using rxtd::fft_utils::WindowFunctionHelper;
using rxtd::option_parsing::Option;
using rxtd::option_parsing::OptionList;

WindowFunctionHelper::WindowCreationFunc WindowFunctionHelper::parse(sview desc, option_parsing::OptionParser parser, Logger& cl) {
	auto description = Option{ desc }.asSequence(L'(', L')', L',');

	if (description.isEmpty()) {
		return [](array_span<float> result) {
			return createRectangular(result);
		};
	}

	auto typeOpt = description.getElement(0).first;
	auto type = typeOpt.asIString();
	auto argsOpt = description.getElement(0).second;

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
		const auto param = parser.parse(argsOpt, typeOpt.asString()).valueOr(3.0f);
		return [=](array_span<float> result) {
			createKaiser(result, param);
		};
	}

	if (type == L"exponential") {
		const auto param = parser.parse(argsOpt, typeOpt.asString()).valueOr(8.69f);
		return [=](array_span<float> result) {
			return createExponential(result, param);
		};
	}

	if (type == L"chebyshev") {
		const auto param = parser.parse(argsOpt, typeOpt.asString()).valueOr(80.0f);
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
	std::fill(result.begin(), result.end(), 1.0f);
}

void WindowFunctionHelper::createCosineSum(array_span<float> result, float a0) {
	float reverseSize = 1.0f / static_cast<float>(result.size());
	for (index i = 0; i < result.size(); ++i) {
		result[i] = a0 - (1.0f - a0) * std::cos(2.0f * std_fixes::MyMath::pi<float>() * static_cast<float>(i) * reverseSize);
	}
}

void WindowFunctionHelper::createKaiser(array_span<float> result, float alpha) {
	const double pia = std_fixes::MyMath::pi<double>() * static_cast<double>(alpha);
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
	// using https://www.dsprelated.com/showarticle/42.php as guide
	// This implementation:
	//	• for even result size generate asymmetric window;
	//	• for odd result size generates symmetric window.
	// This is inconsistent, but in this project all possible expected sizes are even.

	using Float = double;
	using Fft = ComplexFft<Float>;

	index size = static_cast<index>(result.size());
	bool sizeReduced = false;
	if (size % 2 != 0) {
		sizeReduced = true;
		size -= 1;
	}
	const Float sizeF = static_cast<Float>(size);
	const Float sizeNorm = static_cast<Float>(1.0) / sizeF;

	const Float _10_pow_alpha = std::pow(static_cast<Float>(10.0), static_cast<Float>(attenuation) / static_cast<Float>(20.0));
	const Float beta = std::cosh(std::acosh(_10_pow_alpha) * sizeNorm);

	auto W0 = [=](index i)-> Float {
		const auto a = std::abs(beta * std::cos(std_fixes::MyMath::pi<Float>() * static_cast<Float>(i) / sizeF));
		const auto res = a <= static_cast<Float>(1.0)
		                 ? std::cos(sizeF * std::acos(a))
		                 : std::cosh(sizeF * std::acosh(a));
		return (i & 1) != 0 ? -res : res;
	};

	Fft fft;
	fft.setParams(size, true);
	std::vector<Fft::complex_type> freqs;
	freqs.resize(static_cast<size_t>(size));
	for (index i = 0; i < size; ++i) {
		freqs[static_cast<size_t>(i)] = static_cast<Float>(W0(i));
	}
	fft.process(freqs);

	auto generated = fft.getWritableResult();
	generated[0] /= 2.0;

	const auto max = static_cast<float>((*std::max_element(
		generated.begin(), generated.end(),
		[](auto l, auto r) { return l.real() < r.real(); }
	)).real());

	for (index i = 0; i < size; ++i) {
		result[i] = static_cast<float>(generated[i].real()) / max;
	}
	if (sizeReduced) {
		result.back() = result.front();
	}
}
