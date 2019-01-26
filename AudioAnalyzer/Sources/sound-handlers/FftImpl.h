/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../kiss_fft/KissFft.hh"
#include <vector>

namespace rxaa {
	class FftImpl {
	private:
		const unsigned fftSize;
		const double scalar;

		std::vector<float> windowFunction;

		kiss_fft::KissFft<float> kiss;

		size_t inputBufferSize { };
		size_t outputBufferSize { };
		// std::vector<float> inputBuffer;
		// std::vector<decltype(kiss)::cpx_t> outputBuffer;

	public:
		using input_buffer_type = float;
		using output_buffer_type = decltype(kiss)::cpx_t;

	private:
		input_buffer_type* inputBuffer { };
		output_buffer_type* outputBuffer { };

	public:
		static FftImpl* change(FftImpl* old, unsigned newSize);

		FftImpl(unsigned fftSize);

		double getDC() const;
		double getBinMagnitude(unsigned binIndex) const;

		void setBuffers(input_buffer_type* inputBuffer, output_buffer_type* outputBuffer);
		void process(const float* wave);
		size_t getInputBufferSize() const;
		size_t getOutputBufferSize() const;

	private:
		static std::vector<float> createWindowFunction(unsigned int fftSize);
	};
}
