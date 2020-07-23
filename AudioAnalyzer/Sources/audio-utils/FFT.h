/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "kiss_fft/KissFft.hh"
#include "array_view.h"

namespace rxtd::audio_utils {
	class FFT {
		index fftSize{ };
		double scalar{ };

		std::vector<float> window;

		kiss_fft::KissFft<float> kiss;

		index inputBufferSize{ };
		index outputBufferSize{ };
		// std::vector<float> inputBuffer;
		// std::vector<decltype(kiss)::cpx_t> outputBuffer;

	public:
		using input_buffer_type = float;
		using output_buffer_type = decltype(kiss)::cpx_t;

	private:
		array_span<input_buffer_type> inputBuffer{ };
		array_span<output_buffer_type> outputBuffer{ };

	public:
		FFT() = default;
		FFT(index fftSize, bool correctScalar);

		void setSize(index newSize, bool correctScalar);

		double getDC() const;
		double getBinMagnitude(index binIndex) const;

		void setBuffers(array_span<input_buffer_type> inputBuffer, array_span<output_buffer_type> outputBuffer);

		void resetBuffers() {
			inputBuffer = { };
			outputBuffer = { };
		}

		void process(array_view<float> wave);
		index getInputBufferSize() const;
		index getOutputBufferSize() const;

	private:
		static std::vector<float> createHannWindow(index fftSize);
	};
}
