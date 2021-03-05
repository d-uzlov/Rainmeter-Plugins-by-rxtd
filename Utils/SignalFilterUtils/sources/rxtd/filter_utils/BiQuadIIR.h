// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "AbstractFilter.h"

namespace rxtd::filter_utils {
	class BiQuadIIR : public AbstractFilter {
		// inspired by https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.lfilter.html

		double a1{};
		double a2{};
		double b0{};
		double b1{};
		double b2{};

		double state0{};
		double state1{};

		double gainAmp = 1.0;

	public:
		BiQuadIIR() = default;
		BiQuadIIR(double a0, double a1, double a2, double b0, double b1, double b2);

		void apply(array_span<float> signal) override;

		void addGainDbEnergy(double gainDB) override;
	};
}
