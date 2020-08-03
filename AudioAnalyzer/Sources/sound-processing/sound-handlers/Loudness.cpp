/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Loudness.h"

using namespace std::string_literals;
using namespace std::literals::string_view_literals;

using namespace audio_analyzer;

void Loudness::_reset() {
	counter = 0;
	intermediateRmsResult = 0.0;
}

void Loudness::_process(array_view<float> wave, float average) {
	for (double x : wave) {
		intermediateRmsResult += x * x;
		counter++;
		if (counter >= getBlockSize()) {
			finishBlock();
		}
	}
}

void Loudness::finishBlock() {
	if (counter == 0) {
		return;
	}

	const double value = std::sqrt(intermediateRmsResult / counter);
	setNextValue(value);
	counter = 0;
	intermediateRmsResult = 0.0;
}

sview Loudness::getDefaultTransform() const {
	return L"db map[from -70 + 0.691 : 0.691] filter[attack 100, attack 500] clamp"sv;
}
