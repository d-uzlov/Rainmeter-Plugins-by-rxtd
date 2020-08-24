/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <chrono>
#include <functional>

#include "DownsampleHelper.h"
#include "filter-utils/LogarithmicIRF.h"
#include "FFT.h"
#include "GrowingVector.h"

namespace rxtd::audio_utils {
	class FftCascade {
	public:
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		struct Params {
			index fftSize;
			index samplesPerSec;

			double legacy_attackTime;
			double legacy_decayTime;

			index inputStride;
			bool legacy_correctZero;

			std::function<void(array_view<float> result, index cascade)> callback;
		};

	private:
		FftCascade* successorPtr{ };
		FFT* fftPtr{ };

		Params params{ };
		index cascadeIndex{ };

		utils::GrowingVector<float> buffer;
		DownsampleHelper downsampleHelper{ 2 };
		LogarithmicIRF filter{ };
		std::vector<float> values;
		float legacy_dc{ };
		bool hasChanges = false;

	public:
		void setParams(Params _params, FFT* _fftPtr, FftCascade* _successorPtr, index _cascadeIndex);
		void process(array_view<float> wave, clock::time_point killTime);

		[[nodiscard]]
		double legacy_getDC() const {
			return legacy_dc;
		}

	private:
		void resampleResult();
		void doFft(array_view<float> chunk);
	};
}
