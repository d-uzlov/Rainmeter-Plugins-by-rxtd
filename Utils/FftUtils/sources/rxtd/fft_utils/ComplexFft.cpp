// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "ComplexFft.h"

#define PFFFT_ENABLE_FLOAT
#define PFFFT_ENABLE_DOUBLE
#include <libs/pffft/pffft.hpp>

#include "FftSizeHelper.h"

using rxtd::fft_utils::ComplexFft;

template<typename Float>
class ComplexFft<Float>::FftImplWrapper {
public:
	using scalar_type = scalar_type;
	using complex_type = complex_type;

private:
	scalar_type scalar{};

	pffft::Fft<complex_type> fft;

public:
	pffft::AlignedVector<complex_type> inputBuffer;
	pffft::AlignedVector<complex_type> outputBuffer;

public:
	FftImplWrapper();

	void setParams(index size, bool reverse);
	
	void forward(array_view<complex_type> wave);
	void inverse(array_view<complex_type> wave);
};

template<typename Float>
ComplexFft<Float>::FftImplWrapper::FftImplWrapper() : fft(0) {}

template<typename Float>
void ComplexFft<Float>::FftImplWrapper::setParams(index size, bool reverse) {
	if (!(FftSizeHelper::findNextAllowedLength(size, false) == size)) {
		throw std::runtime_error{ "ComplexFft::FftImpl::setParams(): invalid fft size" };
	}
	if (pffft_simd_size() != 4) {
		throw std::runtime_error{ "ComplexFft::FftImpl::setParams(): unexpected pffft_simd_size result" };
	}

	scalar = static_cast<scalar_type>(1.0);
	if (!reverse) {
		scalar /= static_cast<scalar_type>(size); // NOLINT(bugprone-integer-division)
	}

	fft.prepareLength(static_cast<int>(size));

	inputBuffer = fft.valueVector();
	outputBuffer = fft.spectrumVector();
}

template<typename Float>
void ComplexFft<Float>::FftImplWrapper::forward(array_view<complex_type> input) {
	std::copy(input.begin(), input.end(), inputBuffer.begin());
	fft.forward(inputBuffer, outputBuffer);
	for (auto& val : outputBuffer) {
		val *= scalar;
	}
}

template<typename Float>
void ComplexFft<Float>::FftImplWrapper::inverse(array_view<complex_type> input) {
	std::copy(input.begin(), input.end(), inputBuffer.begin());
	fft.inverse(inputBuffer, outputBuffer);
}


template<typename Float>
ComplexFft<Float>::ComplexFft() {
	impl = std::make_unique<FftImplWrapper>();
}

template<typename Float>
ComplexFft<Float>::~ComplexFft() = default;

template<typename Float>
void ComplexFft<Float>::setParams(index size, bool reverse) {
	impl->setParams(size, reverse);
}

template<typename Float>
void ComplexFft<Float>::forward(array_view<complex_type> wave) {
	impl->forward(wave);
}

template<typename Float>
void ComplexFft<Float>::inverse(array_view<complex_type> wave) {
	impl->inverse(wave);
}

template<typename Float>
array_span<typename ComplexFft<Float>::complex_type> ComplexFft<Float>::getWritableResult() {
	return array_span{ impl->outputBuffer.data(), static_cast<index>(impl->outputBuffer.size()) };
}

template<typename Float>
array_view<typename ComplexFft<Float>::complex_type> ComplexFft<Float>::getResult() const {
	return array_span{ impl->outputBuffer.data(), static_cast<index>(impl->outputBuffer.size()) };
}

template class ComplexFft<float>;
template class ComplexFft<double>;
