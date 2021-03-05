// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "MathBitTwiddling.h"

using rxtd::std_fixes::MathBitTwiddling;

double MathBitTwiddling::fastPow(double a, double b) {
	// https://martin.ankerl.com/2007/10/04/optimized-pow-approximation-for-java-and-c-c/

	static_assert(sizeof(double) == 2 * sizeof(int32_t));

	union {
		double d;
		int32_t x[2];
	} u = { a };
	u.x[1] = static_cast<int32_t>(b * (u.x[1] - 1072632447) + 1072632447);
	u.x[0] = 0;

	return u.d;
}

float MathBitTwiddling::fastLog2(float val) {
	// http://www.flipcode.com/archives/Fast_log_Function.shtml

	static_assert(sizeof(float) == sizeof(uint32_t));

	union {
		float fl;
		uint32_t ui;
	} u{ val };

	uint32_t x = u.ui;
	const float log2rough = static_cast<float>((x >> 23) & 0xFF) - 128.0f;
	x &= ~(0xFF << 23);
	x += 0x7F << 23;
	u.ui = x;

	u.fl = ((-1.0f / 3.0f) * u.fl + 2.0f) * u.fl - 2.0f * (1.0f / 3.0f);

	return u.fl + log2rough;
}

float MathBitTwiddling::fastSqrt(float value) {
	// https://bits.stephan-brumme.com/squareRoot.html

	static_assert(sizeof(float) == sizeof(uint32_t));
	union {
		float f;
		uint32_t i;
	} u{ value };

	u.i += 0x7F << 23;
	u.i >>= 1;

	return u.f;
}
