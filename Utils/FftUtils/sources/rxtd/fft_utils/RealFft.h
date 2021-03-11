// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <complex>

namespace rxtd::fft_utils {
	class RealFft {
		class FftImplWrapper;

	public:
		using scalar_type = float;

	private:
		std::unique_ptr<FftImplWrapper> impl;

	public:
		RealFft();
		~RealFft();

		void setParams(index size, array_view<scalar_type> window);

		void fillMagnitudes(array_span<scalar_type> result) const;

		void process(array_view<scalar_type> wave);
	};
}
