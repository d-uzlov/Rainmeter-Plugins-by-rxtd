/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "filter-utils/LogarithmicIRF.h"
#include "FFT.h"
#include "filter-utils/InfiniteResponseFilter.h"

namespace rxtd::audio_utils {
	class FftCascade {
	public:
		struct Params {
			index fftSize;
			index samplesPerSec;

			double legacy_attackTime;
			double legacy_decayTime;

			index inputStride;
			bool correctZero;
		};

	private:
		class RingBuffer {
			std::vector<float> buffer;
			index endOffset = 0;
			index takenOffset = 0;

			static constexpr index filterOrder = 5;
			InfiniteResponseFilterFixed<filterOrder + 1> filter;
			bool isOdd = false;

		public:
			RingBuffer();

			// returns part of the wave that didn't fit info the buffer
			[[nodiscard]]
			array_view<float> fill(array_view<float> wave);

			// returns part of the wave that didn't fit info the buffer
			[[nodiscard]]
			array_view<float> fillResampled(array_view<float> wave);

			void pushOne(float value);

			void shift(index stride);

			[[nodiscard]]
			bool isFull() const {
				return endOffset == index(buffer.size());
			}

			[[nodiscard]]
			array_view<float> getBuffer() const {
				return buffer;
			}

			[[nodiscard]]
			array_view<float> take();

			void setSize(index value) {
				buffer.resize(value);
				endOffset = 0;
				takenOffset = 0;
			}
		};

		FftCascade* successor{ };
		FFT* fft{ };

		Params params{ };
		index cascadeIndex{ };

		RingBuffer buffer;
		LogarithmicIRF filter{ };
		std::vector<float> values;
		float dc{ };

	public:
		void setParams(Params _params, FFT* fft, FftCascade* successor, index cascadeIndex);
		void process(array_view<float> wave);
		void reset();

		[[nodiscard]]
		array_view<float> getValues() const {
			return values;
		}

		[[nodiscard]]
		double getDC() const {
			return dc;
		}

	private:
		void resampleResult();
		void doFft();
	};
}
