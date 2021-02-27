/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "CubicInterpolationHelper.h"

using rxtd::audio_utils::CubicInterpolationHelper;

float CubicInterpolationHelper::getValueFor(float x) {
	const double floor = std::floor(x);
	if (floor < 0.0f) {
		return sourceValues[0];
	}

	const index x1 = std::lround(floor);
	if (x1 + 1 >= sourceValues.size()) {
		return sourceValues.back();
	}

	if (x1 != currentIntervalIndex) {
		currentIntervalIndex = x1;
		calcCoefsFor(x1);
	}

	const double dx = static_cast<double>(x) - floor;
	double xn = 1.0;

	double result = 0.0;
	result += coefs.d;

	xn *= dx;
	result += xn * coefs.c;

	xn *= dx;
	result += xn * coefs.b;

	xn *= dx;
	result += xn * coefs.a;

	return static_cast<float>(result);
}

double CubicInterpolationHelper::calcDerivativeFor(index ind) const {
	//	Derivatives are calculated using Fritsch–Carlson method as in
	//		https://math.stackexchange.com/questions/45218/implementation-of-monotone-cubic-interpolation/51412#51412
	//	to ensure monotonic behavior
	//

	if (ind <= 0) {
		return sourceValues[1] - sourceValues[0];
	}
	if (ind + 1 >= sourceValues.size()) {
		return sourceValues[ind] - sourceValues[ind - 1];
	}

	const double left = sourceValues[ind] - sourceValues[ind - 1];
	const double right = sourceValues[ind + 1] - sourceValues[ind];

	if (left * right <= 0.0) {
		return 0.0;
	}

	return 6.0 / (3.0 / left + 3.0 / right);
}

void CubicInterpolationHelper::calcCoefsFor(index ind) {
	// Solving:
	// f(x) == a*x^3 + b*x^2 + c*x + d
	// f(x1) == v1
	// f(x2) == v2
	// f'(x1) == q1
	// f'(x2) == q2
	// 
	// a*x1^3 + b*x1^2 + c*x1 + d == v1
	// a*x2^3 + b*x2^2 + c*x2 + d == v2
	// 3*a*x1^2 + 2*b*x1 + c == q1
	// 3*a*x2^2 + 2*b*x2 + c == q2
	//
	// We shift x1 to 0.0 and x2 to 1.0 and get:
	//	d == v1
	//	a + b + c + d == v2
	//	c == q1
	//	3*a + 2*b + c == q2
	//
	//	a + b == v2 - c - d == v2 - q1 - v1
	//	3*a + 2*b == q2 - c == q2 - q1
	//	a == (q2 - q1) - (v2 - q1 - v1) * 2
	//	  == 2*v1 - 2*v2 + q1 + q2
	//	b == v2 - q1 - v1 - a
	//	  == v2 - q1 - v1 - (2*v1 - 2*v2 + q1 + q2)
	//	  == -3*v1 + 3*v2 - 2*q1 - q2
	//
	//	Let's put q1 == 0.0 in the leftmost point and q2 == 0.0 in the rightmost point
	//	

	const double v1 = sourceValues[ind];
	const double v2 = sourceValues[ind + 1];
	// const double q1 = ind - 1 < 0 ? 0.0 : (sourceValues[ind + 1] - sourceValues[ind - 1]) * 0.5;
	// const double q2 = ind + 2 >= sourceValues.size() ? 0.0 : (sourceValues[ind + 2] - sourceValues[ind]) * 0.5;
	const double q1 = calcDerivativeFor(ind);
	const double q2 = calcDerivativeFor(ind + 1);


	coefs.a = 2.0 * (v1 - v2) + q1 + q2;
	coefs.b = v2 - v1 - q1 - coefs.a;
	coefs.c = q1;
	coefs.d = v1;
}
