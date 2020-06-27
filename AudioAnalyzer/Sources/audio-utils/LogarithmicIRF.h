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
		double attackDecayConstants[2] { };
		double result = 0.0;

	public:
		void setParams(double attackTime, double decayTime, index samplesPerSec, index stride);
		void setParams(double attackTime, double decayTime, index samplesPerSec);

		double next(double value);
		double apply(double prev, double value);
		double getLastResult() const;

		void reset();

	private:
		static double calculateAttackDecayConstant(double time, index samplesPerSec, index stride);
	};

}
