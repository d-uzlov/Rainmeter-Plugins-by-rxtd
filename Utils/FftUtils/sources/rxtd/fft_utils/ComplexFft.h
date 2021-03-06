// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <libs/kiss_fft/KissFft.hh>

namespace rxtd::fft_utils {
	class ComplexFft {
		using FftImpl = kiss_fft::KissFft<float>;

	public:
		using scalar_type = FftImpl::scalar_type;
		using complex_type = FftImpl::complex_type;

	private:
		index size{};
		scalar_type scalar{};

		FftImpl kiss;

		std::vector<complex_type> outputBuffer;

	public:
		ComplexFft() = default;

		void setParams(index _size, bool reverse = false);

		[[nodiscard]]
		array_view<complex_type> getResult() const {
			return outputBuffer;
		}

		void process(array_view<complex_type> input);
	};
}
