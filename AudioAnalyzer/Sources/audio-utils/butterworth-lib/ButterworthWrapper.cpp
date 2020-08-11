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
		wrapCoefs(dcof_bwlp, order + 1, order, digitalCutoff, 0.0),
		wrapCoefs(ccof_bwlp, order + 1, order, digitalCutoff, 0.0),
		sf_bwlp(order, digitalCutoff, 0.0)
	};
}

FilterParameters ButterworthWrapper::calcCoefHighPass(index _order, double digitalCutoff) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	digitalCutoff = std::clamp(digitalCutoff, 0.01, 1.0 - 0.01);

	return {
		wrapCoefs(dcof_bwhp, order + 1, order, digitalCutoff, 0.0),
		wrapCoefs(ccof_bwhp, order + 1, order, digitalCutoff, 0.0),
		sf_bwhp(order, digitalCutoff, 0.0)
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
		wrapCoefs(dcof_bwbp, 2 * order + 1, order, digitalCutoffLow, digitalCutoffHigh),
		wrapCoefs(ccof_bwbp, 2 * order + 1, order, digitalCutoffLow, digitalCutoffHigh),
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
		wrapCoefs(dcof_bwbs, 2 * order + 1, order, digitalCutoffLow, digitalCutoffHigh),
		wrapCoefs(ccof_bwbs, 2 * order + 1, order, digitalCutoffLow, digitalCutoffHigh),
		sf_bwbs(order, digitalCutoffLow, digitalCutoffHigh)
	};
}
