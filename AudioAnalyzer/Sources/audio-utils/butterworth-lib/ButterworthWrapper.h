/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../filter-utils/InfiniteResponseFilter.h"

namespace rxtd::audio_utils {
	class ButterworthWrapper {
		// https://stackoverflow.com/questions/10373184/bandpass-butterworth-filter-implementation-in-c
		// http://www.exstrom.com/journal/sigproc/

	public:
		[[nodiscard]]
		static FilterParameters calcCoefLowPass(index order, double samplingFrequency, double cutoffFrequency);

		[[nodiscard]]
		static FilterParameters
		calcCoefLowPass(index order, double samplingFrequency, double cutoffFrequency, double unused) {
			return calcCoefLowPass(order, samplingFrequency, cutoffFrequency);
		}

		[[nodiscard]]
		static FilterParameters calcCoefHighPass(index order, double samplingFrequency, double cutoffFrequency);

		[[nodiscard]]
		static FilterParameters
		calcCoefHighPass(index order, double samplingFrequency, double cutoffFrequency, double unused) {
			return calcCoefHighPass(order, samplingFrequency, cutoffFrequency);
		}

		[[nodiscard]]
		static FilterParameters calcCoefBandPass(
			index order,
			double samplingFrequency,
			double lowerCutoffFrequency, double upperCutoffFrequency
		);

		[[nodiscard]]
		static FilterParameters calcCoefBandStop(
			index order,
			double samplingFrequency,
			double lowerCutoffFrequency, double upperCutoffFrequency
		);
	};
}
