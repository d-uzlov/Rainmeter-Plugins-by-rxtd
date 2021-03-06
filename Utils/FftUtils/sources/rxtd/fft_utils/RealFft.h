// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <libs/kiss_fft/KissFft.hh>

namespace rxtd::fft_utils {
	class RealFft {
		using FftImpl = kiss_fft::KissFft<float>;

	public:
		using scalar_type = FftImpl::scalar_type;
		using complex_type = FftImpl::complex_type;

	private:
		index size{};
		scalar_type scalar{};

		std::vector<scalar_type> window;

		FftImpl kiss;

		// need separate input buffer because of window application
		std::vector<scalar_type> inputBuffer;
		std::vector<complex_type> outputBuffer;

	public:
		RealFft() = default;

		void setParams(index _size, std::vector<scalar_type> window);

		void fillMagnitudes(array_span<scalar_type> result) const;

		void process(array_view<scalar_type> wave);
	};
}
