/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BiQuadIIR.h"
#include "Math.h"

#include "undef.h"

using namespace audio_utils;

BiQuadIIR::BiQuadIIR(double _a0, double _a1, double _a2, double _b0, double _b1, double _b2) {
	a1 = _a1 / _a0;
	a2 = _a2 / _a0;
	b0 = _b0 / _a0;
	b1 = _b1 / _a0;
	b2 = _b2 / _a0;

	state0 = 0.0;
	state1 = 0.0;
}

void BiQuadIIR::apply(array_span<float> signal) {
	for (float& value : signal) {
		const double valueFiltered = b0 * value + state0;
		state0 = b1 * value - a1 * valueFiltered + state1;
		state1 = b2 * value - a2 * valueFiltered;
		value = valueFiltered;
	}
}

void BiQuadIIR::addGain(double gainDB) {
	const double gain = utils::Math::dbToGain(gainDB);
	b0 *= gain;
	b1 *= gain;
	b2 *= gain;
}
