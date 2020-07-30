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
		float attackDecayConstants[2]{ };
		float result = 0.0;

	public:
		void setParams(double attackTime, double decayTime, index samplesPerSec, index stride) {
			attackDecayConstants[0] = calculateAttackDecayConstant(float(attackTime), samplesPerSec, stride);
			attackDecayConstants[1] = calculateAttackDecayConstant(float(decayTime), samplesPerSec, stride);
		}

		void setParams(double attackTime, double decayTime, index samplesPerSec) {
			setParams(attackTime, decayTime, samplesPerSec, 1);
		}

		float next(float value) {
			result = value + attackDecayConstants[(value < result)] * (result - value);
			return result;
		}

		[[nodiscard]]
		float apply(float prev, float value) {
			return value + attackDecayConstants[(value < prev)] * (prev - value);
		}

		[[nodiscard]]
		float getLastResult() const {
			return result;
		}

		void reset() {
			result = 0.0;
		}

	private:
		[[nodiscard]]
		static float calculateAttackDecayConstant(float time, index samplesPerSec, index stride) {
			// stride and samplesPerSec are semantically guaranteed to be positive
			// time can be positive or zero
			// In case of zero result is exp(-inf) == 0 which is totally fine
			return std::exp(-2.0f * stride / (samplesPerSec * time));
		}
	};

}
