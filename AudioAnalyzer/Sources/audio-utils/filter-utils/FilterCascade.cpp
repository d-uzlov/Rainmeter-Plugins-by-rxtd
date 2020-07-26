/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FilterCascade.h"

using namespace audio_utils;

void FilterCascade::apply(array_view<float> wave) {
	processed.resize(wave.size());
	std::copy(wave.begin(), wave.end(), processed.begin());

	for (const auto& filterPtr : filters) {
		filterPtr->apply(processed);
	}
}
