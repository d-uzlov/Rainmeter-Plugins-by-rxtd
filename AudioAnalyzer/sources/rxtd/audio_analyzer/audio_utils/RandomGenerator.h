// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include <random>

namespace rxtd::audio_analyzer::audio_utils {
	class RandomGenerator {
		std::random_device rd;
		std::mt19937 e2;
		std::uniform_real_distribution<> dist;

	public:
		RandomGenerator() : e2(rd()), dist(-1.0, 1.0) { }

		[[nodiscard]]
		double next() {
			return dist(e2);
		}
	};
}
