// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "RealFft.h"

#define PFFFT_ENABLE_FLOAT
#define PFFFT_ENABLE_DOUBLE
#include <libs/pffft/pffft.hpp>

#include "FftSizeHelper.h"

using rxtd::fft_utils::RealFft;

class RealFft::FftImplWrapper {
public:
	using scalar_type = scalar_type;

private:
	pffft::AlignedVector<scalar_type> window;
	// need separate input buffer because of window application
	pffft::AlignedVector<scalar_type> inputBuffer;
	pffft::AlignedVector<pffft::Fft<scalar_type>::Complex> outputBuffer;

	scalar_type scalar{};

	pffft::Fft<scalar_type> fft;

public:
	FftImplWrapper();

	void setParams(index size, array_view<scalar_type> window);

	void fillMagnitudes(array_span<scalar_type> result) const;

	void process(array_view<scalar_type> wave);
};

RealFft::FftImplWrapper::FftImplWrapper() : fft(0) {}

void RealFft::FftImplWrapper::setParams(index size, array_view<scalar_type> _window) {
	if (!(FftSizeHelper::findNextAllowedLength(size, true) == size)) {
		throw std::runtime_error{ "RealFft2::FftImpl::setParams(): invalid fft size" };
	}
	if (pffft_simd_size() != 4) {
		throw std::runtime_error{ "RealFft2::FftImpl::setParams(): unexpected pffft_simd_size result" };
	}
	if (const index windowSize = static_cast<index>(_window.size());
		windowSize != size && windowSize != 0) {
		throw std::runtime_error{ "RealFft2::FftImpl::setParams(): invalid window size" };
	}

	scalar = 1.0f / static_cast<float>(size / 2); // NOLINT(bugprone-integer-division)
	window.resize(static_cast<size_t>(_window.size()));
	std::copy(_window.begin(), _window.end(), window.begin());

	fft.prepareLength(static_cast<int>(size));

	inputBuffer = fft.valueVector();
	outputBuffer = fft.spectrumVector();
}

void RealFft::FftImplWrapper::fillMagnitudes(array_span<scalar_type> result) const {
	for (index i = 0; i < result.size(); ++i) {
		const auto v = outputBuffer[static_cast<size_t>(i)];
		const float square = v.real() * v.real() + v.imag() * v.imag();
		result[i] = std::sqrt(square) * scalar;
	}
}

void RealFft::FftImplWrapper::process(array_view<float> wave) {
	if (!window.empty()) {
		for (index i = 0; i < wave.size(); ++i) {
			inputBuffer[static_cast<size_t>(i)] = wave[i] * window[static_cast<size_t>(i)];
		}
	} else {
		std::copy(wave.begin(), wave.end(), inputBuffer.begin());
	}
	fft.forward(inputBuffer, outputBuffer);
}


RealFft::RealFft() {
	impl = std::make_unique<FftImplWrapper>();
}

RealFft::~RealFft() = default;

void RealFft::setParams(index size, array_view<scalar_type> window) {
	impl->setParams(size, window);
}

void RealFft::fillMagnitudes(array_span<scalar_type> result) const {
	impl->fillMagnitudes(result);
}

void RealFft::process(array_view<scalar_type> wave) {
	impl->process(wave);
}
