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

	public:
		struct AB {
			std::vector<double> a;
			std::vector<double> b;
		};

		static AB calcCoefLowPass(index order, double cutoffFrequency, double samplingFrequency);

		template <index order>
		static InfiniteResponseFilterFixed<order + 1> createLowPass(double centralFrequency, double samplingFrequency) {
			auto [a, b] = calcCoefLowPass(order, centralFrequency, samplingFrequency);
			return { a, b };
		}

		static AB calcCoefHighPass(index order, double cutoffFrequency, double samplingFrequency);

		template <index order>
		static InfiniteResponseFilterFixed<order + 1>
		createHighPass(double centralFrequency, double samplingFrequency) {
			auto [a, b] = calcCoefHighPass(order, centralFrequency, samplingFrequency);
			return { a, b };
		}

		static AB calcCoefBandPass(
			index order,
			double lowerCutoffFrequency, double upperCutoffFrequency,
			double samplingFrequency
		);

		template <index order>
		static InfiniteResponseFilterFixed<order + 1> createBandPass(
			double lowerCutoffFrequency, double upperCutoffFrequency,
			double samplingFrequency) {
			auto [a, b] = calcCoefBandPass(order, lowerCutoffFrequency, upperCutoffFrequency, samplingFrequency);
			return { a, b };
		}

		static AB calcCoefBandStop(index order, double lowerCutoffFrequency, double upperCutoffFrequency, double samplingFrequency);

		template <index order>
		static InfiniteResponseFilterFixed<order + 1>
		createBandStop(double lowerCutoffFrequency, double upperCutoffFrequency, double samplingFrequency) {
			auto [a, b] = calcCoefBandStop(order, lowerCutoffFrequency, upperCutoffFrequency, samplingFrequency);
			return { a, b };
		}
	};
}
