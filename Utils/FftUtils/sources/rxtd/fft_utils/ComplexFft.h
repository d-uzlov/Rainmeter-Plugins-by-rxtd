// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include <libs/kiss_fft/KissFft.hh>

namespace rxtd::fft_utils {
	template<typename Float>
	class ComplexFft {
		using FftImpl = kiss_fft::KissFft<Float>;

	public:
		using scalar_type = typename FftImpl::scalar_type;
		using complex_type = typename FftImpl::complex_type;

	private:
		index size{};
		scalar_type scalar{};

		FftImpl kiss;

		std::vector<complex_type> outputBuffer;

	public:
		ComplexFft() = default;

		void setParams(index _size, bool reverse = false) {
			size = _size;
			scalar = scalar_type(1.0);
			if (!reverse) {
				scalar /= static_cast<scalar_type>(size);
			}

			// Apparently KISS FFT doesn't handle assignment with only "inverse" argument changed.
			// Tests of ComplexFft class are failing when .assign is used.
			// kiss.assign(static_cast<std::size_t>(size), reverse);
			kiss = { static_cast<std::size_t>(size), reverse };

			outputBuffer.resize(static_cast<size_t>(size));
		}

		[[nodiscard]]
		array_span<complex_type> getWritableResult() {
			return outputBuffer;
		}

		[[nodiscard]]
		array_view<complex_type> getResult() const {
			return outputBuffer;
		}

		void process(array_view<complex_type> input) {
			kiss.transform(input.data(), outputBuffer.data());
			for (auto& val : outputBuffer) {
				val *= scalar;
			}
		}
	};
}
