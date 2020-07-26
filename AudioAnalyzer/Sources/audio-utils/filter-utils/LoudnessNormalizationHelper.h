/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "array_view.h"
#include "BiQuadIIR.h"
#include "InfiniteResponseFilter.h"

namespace rxtd::audio_utils {
	// Based on ReplayGain algorithm curve
	// I approximated the filter cascade
	// using ReplayGain curve picture and frequency response curves of filters
	// Probably nowhere near close to proper implementation,
	// but it works good enough for me
	// and, most importantly, I mostly understand how this works
	class LoudnessNormalizationHelper {
		static constexpr index BWOrder = 5;

		BiQuadIIR filter1{ };
		BiQuadIIR filter2{ };
		BiQuadIIR filter3{ };
		InfiniteResponseFilterFixed<BWOrder + 1> filter4{ };

		std::vector<float> processed;

	public:
		LoudnessNormalizationHelper() = default;

		LoudnessNormalizationHelper(double samplingFrequency);

		void apply(array_view<float> wave);

		array_view<float> getProcessed() const {
			return processed;
		}

	private:
		static BiQuadIIR createFilter1(double samplingFrequency);

		static BiQuadIIR createFilter2(double samplingFrequency);

		static BiQuadIIR createFilter3(double samplingFrequency);

		static InfiniteResponseFilterFixed<BWOrder + 1> createFilter4(double samplingFrequency);
	};

}
