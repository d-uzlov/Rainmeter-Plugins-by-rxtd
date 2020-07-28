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

namespace rxtd::audio_utils {
	class LogarithmicIRF {
		float attackDecayConstants[2] { };
		float result = 0.0;

	public:
		void setParams(double attackTime, double decayTime, index samplesPerSec, index stride);
		void setParams(double attackTime, double decayTime, index samplesPerSec);

		float next(float value);
		float apply(float prev, float value);
		float getLastResult() const;

		void reset();

	private:
		static float calculateAttackDecayConstant(float time, index samplesPerSec, index stride);
	};

}
