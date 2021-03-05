// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd::audio_analyzer::audio_utils {
	class CubicInterpolationHelper {
		array_view<float> sourceValues;

		struct {
			double a;
			double b;
			double c;
			double d;
		} coefs{};

		index currentIntervalIndex{};

	public:
		void setSource(array_view<float> value) {
			sourceValues = value;
			currentIntervalIndex = -1;
		}

		[[nodiscard]]
		float getValueFor(float x);

	private:
		[[nodiscard]]
		double calcDerivativeFor(index ind) const;

		// ind should be < sourceValues.size() - 1
		void calcCoefsFor(index ind);
	};
}
