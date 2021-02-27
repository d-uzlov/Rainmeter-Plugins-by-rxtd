/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::filter_utils {
	class LogarithmicIRF {
		float attackDecayConstants[2]{};
		float result = 0.0;

	public:
		void setParams(double attackTime, double decayTime, index sampleRate, index stride) {
			attackDecayConstants[0] = calculateAttackDecayConstant(float(attackTime), sampleRate, stride);
			attackDecayConstants[1] = calculateAttackDecayConstant(float(decayTime), sampleRate, stride);
		}

		void setParams(double attackTime, double decayTime, index sampleRate) {
			setParams(attackTime, decayTime, sampleRate, 1);
		}

		float next(float value) {
			result = apply(result, value);
			return result;
		}

		[[nodiscard]]
		float apply(float prev, float value) const {
			// if infinity or nan, then return without any changes
			const float delta = prev - value;
			if (!(delta > -std::numeric_limits<float>::max()
				&& delta < std::numeric_limits<float>::max())) {
				return value;
			}
			if (std::abs(delta) < 1.0e-30f) {
				return value;
			}

			return value + attackDecayConstants[(value < prev)] * delta;
		}

		void arrayApply(array_view<float> source, array_span<float> dest) const {
			for (int i = 0; i < source.size(); ++i) {
				dest[i] = apply(dest[i], source[i]);
			}
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
		static float calculateAttackDecayConstant(float time, index sampleRate, index stride) {
			// stride and samplesPerSec are semantically guaranteed to be positive
			// time can be positive or zero
			// In case of zero result is exp(-inf) == 0 which is totally fine
			return std::exp(-2.0f * stride / (sampleRate * time));
		}
	};
}
