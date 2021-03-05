// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once

namespace rxtd::std_fixes {
	class MathBitTwiddling {
	public:
		// returns approximately a**b
		[[nodiscard]]
		static double fastPow(double a, double b);
		
		[[nodiscard]]
		static float fastLog2(float val);
		
		[[nodiscard]]
		static float fastSqrt(float value);
	};
}
