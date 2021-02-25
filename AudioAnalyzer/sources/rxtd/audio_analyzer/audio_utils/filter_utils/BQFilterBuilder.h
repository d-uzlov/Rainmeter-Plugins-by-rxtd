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

namespace rxtd::audio_analyzer::audio_utils::filter_utils {
	// Based on formulas from Audio-EQ-Cookbook
	class BQFilterBuilder {
	public:
		[[nodiscard]]
		static BiQuadIIR createHighShelf(double samplingFrequency, double q, double centralFrequency, double dbGain);

		[[nodiscard]]
		static BiQuadIIR createLowShelf(double samplingFrequency, double q, double centralFrequency, double dbGain);

		[[nodiscard]]
		static BiQuadIIR createHighPass(double samplingFrequency, double q, double centralFrequency);

		[[nodiscard]]
		static BiQuadIIR createHighPass(double samplingFrequency, double q, double centralFrequency, double unused) {
			return createHighPass(samplingFrequency, q, centralFrequency);
		}

		[[nodiscard]]
		static BiQuadIIR createLowPass(double samplingFrequency, double q, double centralFrequency);

		[[nodiscard]]
		static BiQuadIIR createLowPass(double samplingFrequency, double q, double centralFrequency, double unused) {
			return createLowPass(samplingFrequency, q, centralFrequency);
		}

		[[nodiscard]]
		static BiQuadIIR createPeak(double samplingFrequency, double q, double centralFrequency, double dbGain);
	};
}
