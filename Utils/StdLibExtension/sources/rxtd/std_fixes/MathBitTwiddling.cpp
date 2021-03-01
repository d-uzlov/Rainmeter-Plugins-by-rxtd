/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "MathBitTwiddling.h"

using rxtd::std_fixes::MathBitTwiddling;

double MathBitTwiddling::fastPow(double a, double b) {
	// https://martin.ankerl.com/2007/10/04/optimized-pow-approximation-for-java-and-c-c/

	static_assert(sizeof(double) == 2 * sizeof(int32_t));

	union {
		double d;
		int32_t x[2];
	} u = { a };
	u.x[1] = static_cast<int>(b * (u.x[1] - 1072632447) + 1072632447);
	u.x[0] = 0;

	return u.d;
}

float MathBitTwiddling::fastLog2(float val) {
	// http://www.flipcode.com/archives/Fast_log_Function.shtml

	static_assert(sizeof(float) == sizeof(uint32_t));

	union {
		float fl;
		uint32_t ui;
	} u;
	
	u.fl = val;

	uint32_t x = u.ui;
	const uint32_t log_2 = ((x >> 23) & 255) - 128;
	x &= ~(255 << 23);
	x += 127 << 23;
	u.ui = x;

	u.fl = ((-1.0f / 3.0f) * u.fl + 2.0f) * u.fl - 2.0f * (1.0f / 3.0f);

	return u.fl + static_cast<float>(log_2);
}

float MathBitTwiddling::fastSqrt(float value) {
	// https://bits.stephan-brumme.com/squareRoot.html

	static_assert(sizeof(float) == sizeof(uint32_t));
	union {
		float f;
		uint32_t i;
	} u{ value };

	u.i += 127 << 23;
	u.i >>= 1;

	return u.f;
}