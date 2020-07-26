/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
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

		inline const static string description = L"bqHighPass[0.5, 310] bqPeak[4, 1125, -4.1] bqPeak[4.0, 2665, 5.5] bwLowPass[5, 20000]";

	public:
		static FilterCascade getInstance(double samplingFrequency);
	};

}
