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
	class WeightingFilterBuilder {
	public:
		static BiQuadIIR createKWHighShelf(double samplingFrequency);
		static BiQuadIIR createKWHighPass(double samplingFrequency);

		static BiQuadIIR createHighShelf_DeMan(double dbGain, double Q, double centralFrequency, double samplingFrequency);
		static BiQuadIIR createHighPass_DeMan(double Q, double centralFrequency, double samplingFrequency);
	};
}
