// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "BiQuadIIR.h"
#include "rxtd/std_fixes/MyMath.h"

using rxtd::filter_utils::BiQuadIIR;
using rxtd::std_fixes::MyMath;

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
		value = static_cast<float>(valueFiltered * gainAmp);
	}
}

void BiQuadIIR::addGainDbEnergy(double gainDB) {
	const double gain = MyMath::db2amplitude(gainDB * 0.5);
	gainAmp *= gain;
}
