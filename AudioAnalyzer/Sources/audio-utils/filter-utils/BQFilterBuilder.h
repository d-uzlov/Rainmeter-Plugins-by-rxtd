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

namespace rxtd::audio_utils {
	// Based on formulas from Audio-EQ-Cookbook
	class BQFilterBuilder {
	public:
		[[nodiscard]]
		static BiQuadIIR createHighShelf(double dbGain, double q, double centralFrequency, double samplingFrequency);
		[[nodiscard]]
		static BiQuadIIR createLowShelf(double dbGain, double q, double centralFrequency, double samplingFrequency);

		[[nodiscard]]
		static BiQuadIIR createHighPass(double q, double centralFrequency, double samplingFrequency);
		[[nodiscard]]
		static BiQuadIIR createLowPass(double q, double centralFrequency, double samplingFrequency);

		[[nodiscard]]
		static BiQuadIIR createPeak(double dbGain, double q, double centralFrequency, double samplingFrequency);
	};
}
