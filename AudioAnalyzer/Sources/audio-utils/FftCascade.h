/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../sound-processing/sound-handlers/SoundHandler.h"
#include "filter-utils/LogarithmicIRF.h"
#include "FFT.h"

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
		// FftAnalyzer* parent { };
		FftCascade* successor{ };
		FFT* fft{ };

		Params params{ };

		LogarithmicIRF filter{ };
		std::vector<float> ringBuffer;
		std::vector<float> values;
		index ringBufferOffset{ };
		index transferredElements{ };
		float odd{ };
		float dc{ };
		// Fourier transform looses energy due to downsample, so we multiply result of FFT by (2^0.5)^countOfDownsampleIterations
		float downsampleGain{ };

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
		void doFft();
		void processResampled(array_view<float> wave);
	};
}
