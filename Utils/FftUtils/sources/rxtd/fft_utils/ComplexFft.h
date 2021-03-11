// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <complex>

namespace rxtd::fft_utils {
	template<typename Float>
	class ComplexFft {
		static_assert(std::is_same<Float, float>::value || std::is_same<Float, double>::value, "only float and double are allowed in ComplexFft class");

		class FftImplWrapper;

	public:
		using scalar_type = Float;
		using complex_type = std::complex<scalar_type>;

	private:
		std::unique_ptr<FftImplWrapper> impl;

	public:
		ComplexFft();
		~ComplexFft();

		void setParams(index size, bool reverse);

		[[nodiscard]]
		array_span<complex_type> getWritableResult();

		[[nodiscard]]
		array_view<complex_type> getResult() const;

		void forward(array_view<complex_type> input);

		void inverse(array_view<complex_type> input);
	};
}
