/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Resampler.h"

#include "undef.h"

using namespace audio_analyzer;

void Resampler::setSourceRate(index value) {
	sourceRate = value;
	updateValues();
}

void Resampler::setTargetRate(index value) {
	targetRate = value;
	updateValues();
}

void Resampler::resample(array_view<float> from, array_span<float> to) const {
	if (divide <= 1) {
		std::copy(from.begin(), from.end(), to.begin());
		return;
	}

	const auto newCount = from.size() / divide;
	for (index i = 0; i < newCount; ++i) {
		double value = 0.0;
		for (index j = 0; j < divide; ++j) {
			value += from[i * divide + j];
		}
		to[i] = static_cast<float>(value / divide);
	}
}

void Resampler::updateValues() {
	if (targetRate == 0) {
		divide = 1;
	} else {
		const auto ratio = static_cast<double>(sourceRate) / targetRate;
		if (ratio > 1) {
			divide = static_cast<index>(ratio);
		} else {
			divide = 1;
		}
	}
	sampleRate = sourceRate / divide;
}
