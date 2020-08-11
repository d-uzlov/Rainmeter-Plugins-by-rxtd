/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ButterworthWrapper.h"
#include "iir.h"

using namespace audio_utils;

FilterParameters ButterworthWrapper::calcCoefLowPass(index _order, double digitalCutoff) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoff = std::clamp(digitalCutoff, 0.01, 1.0 - 0.01);

	return {
		wrapCoefs(dcof_bwlp, order, digitalCutoff),
		wrapCoefs(ccof_bwlp, order),
		sf_bwlp(order, digitalCutoff)
	};
}

FilterParameters ButterworthWrapper::calcCoefHighPass(index _order, double digitalCutoff) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoff = std::clamp(digitalCutoff, 0.01, 1.0 - 0.01);

	return {
		wrapCoefs(dcof_bwhp, order, digitalCutoff),
		wrapCoefs(ccof_bwhp, order),
		sf_bwhp(order, digitalCutoff)
	};
}

FilterParameters ButterworthWrapper::calcCoefBandPass(index _order, double digitalCutoffLow, double digitalCutoffHigh) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoffLow = std::clamp(digitalCutoffLow, 0.01, 1.0 - 0.01);
	digitalCutoffHigh = std::clamp(digitalCutoffHigh, 0.01, 1.0 - 0.01);

	return {
		wrapCoefs(dcof_bwbp, order, digitalCutoffLow, digitalCutoffHigh),
		wrapCoefs(ccof_bwbp, order),
		sf_bwbp(order, digitalCutoffLow, digitalCutoffHigh)
	};
}

FilterParameters ButterworthWrapper::calcCoefBandStop(index _order, double digitalCutoffLow, double digitalCutoffHigh) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoffLow = std::clamp(digitalCutoffLow, 0.01, 1.0 - 0.01);
	digitalCutoffHigh = std::clamp(digitalCutoffHigh, 0.01, 1.0 - 0.01);

	return {
		wrapCoefs(dcof_bwbs, order, digitalCutoffLow, digitalCutoffHigh),
		wrapCoefs(ccof_bwbs, order, digitalCutoffLow, digitalCutoffHigh),
		sf_bwbs(order, digitalCutoffLow, digitalCutoffHigh)
	};
}
