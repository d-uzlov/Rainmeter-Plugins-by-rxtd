// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "libs/kiss_fft/KissFft.hh"

namespace rxtd::fft_utils {
	class FFT {
		using FftImpl = kiss_fft::KissFft<float>;

		index fftSize{};
		float scalar{};

		std::vector<float> window;

		FftImpl kiss;

		// need separate input buffer because of window application
		std::vector<FftImpl::scalar_type> inputBuffer;
		std::vector<FftImpl::complex_type> outputBuffer;

	public:
		FFT() = default;

		void setParams(index newSize, std::vector<float> window);

		[[nodiscard]]
		float getDC() const;

		[[nodiscard]]
		float getBinMagnitude(index binIndex) const;

		void process(array_view<float> wave);
	};
}
