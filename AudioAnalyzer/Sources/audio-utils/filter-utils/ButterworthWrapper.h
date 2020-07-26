/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "InfiniteResponseFilter.h"
#include "../butterworth-lib/iir.h"

namespace rxtd::audio_utils {
	class ButterworthWrapper {
	public:
		static InfiniteResponseFilter create(index order, double centralFrequency, double samplingFrequency);

		template<index order>
		static InfiniteResponseFilterFixed<order + 1> createFixed(double centralFrequency, double samplingFrequency) {
			const double digitalFreq = centralFrequency / samplingFrequency;

			double* aCoef = dcof_bwlp(order, digitalFreq);
			int* bCoef = ccof_bwlp(order);
			const double scalingFactor = sf_bwlp(order, digitalFreq);

			std::vector<double> b;
			b.resize(order + 1);
			for (index i = 0; i < index(b.size()); ++i) {
				b[i] = bCoef[i] * scalingFactor;
			}

			std::vector<double> a;
			a.resize(order + 1);
			for (index i = 0; i < index(a.size()); ++i) {
				a[i] = aCoef[i];
			}

			free(aCoef);
			free(bCoef);

			return { a, b };
		}
	};
}
