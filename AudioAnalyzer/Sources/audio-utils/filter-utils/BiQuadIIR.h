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
#include "AbstractFilter.h"

namespace rxtd::audio_utils {
	class BiQuadIIR : public AbstractFilter {
		// inspired by https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html

		double a1;
		double a2;
		double b0;
		double b1;
		double b2;

		double state0;
		double state1;

	public:
		BiQuadIIR() = default;
		BiQuadIIR(double a0, double a1, double a2, double b0, double b1, double b2);

		void apply(array_span<float> signal) override;

		void addGain(double gainDB);
	};
}
