/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "BiQuadIIR.h"
#include "InfiniteResponseFilter.h"
#include "FilterCascade.h"

namespace rxtd::audio_utils {
	// Based on ReplayGain algorithm curve
	// I approximated the filter cascade
	// using ReplayGain curve picture and frequency response curves of filters
	// Probably nowhere near close to proper implementation,
	// but it works good enough for me
	// and, most importantly, I mostly understand how this works
	class LoudnessNormalizationHelper {
		static constexpr index BWOrder = 5;

	public:
		static FilterCascade getInstance(double samplingFrequency);

	private:
		static BiQuadIIR createFilter1(double samplingFrequency);

		static BiQuadIIR createFilter2(double samplingFrequency);

		static BiQuadIIR createFilter3(double samplingFrequency);

		static InfiniteResponseFilterFixed<BWOrder + 1> createFilter4(double samplingFrequency);
	};

}
