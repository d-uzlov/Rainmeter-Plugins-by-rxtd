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

namespace rxtd::audio_utils {
	class LoudnessNormalizationHelper {
		BiQuadIIR filter1{ };
		BiQuadIIR filter2{ };
		BiQuadIIR filter3{ };

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
	};

}
