// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "BiQuadIIR.h"

namespace rxtd::filter_utils {
	// Based on formulas from Audio-EQ-Cookbook
	class BQFilterBuilder {
	public:
		[[nodiscard]]
		static BiQuadIIR createHighShelf(double samplingFrequency, double q, double centralFrequency, double dbGain);

		[[nodiscard]]
		static BiQuadIIR createLowShelf(double samplingFrequency, double q, double centralFrequency, double dbGain);

		[[nodiscard]]
		static BiQuadIIR createHighPass(double samplingFrequency, double q, double centralFrequency);

		[[nodiscard]]
		static BiQuadIIR createHighPass(double samplingFrequency, double q, double centralFrequency, double unused) {
			return createHighPass(samplingFrequency, q, centralFrequency);
		}

		[[nodiscard]]
		static BiQuadIIR createLowPass(double samplingFrequency, double q, double centralFrequency);

		[[nodiscard]]
		static BiQuadIIR createLowPass(double samplingFrequency, double q, double centralFrequency, double unused) {
			return createLowPass(samplingFrequency, q, centralFrequency);
		}

		[[nodiscard]]
		static BiQuadIIR createPeak(double samplingFrequency, double q, double centralFrequency, double dbGain);
	};
}
