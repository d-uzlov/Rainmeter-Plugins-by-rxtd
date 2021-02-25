/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FilterCascade.h"

using rxtd::audio_analyzer::audio_utils::filter_utils::FilterCascade;

void FilterCascade::apply(array_view<float> wave) {
	wave.transferToVector(processed);
	applyInPlace(processed);
}

void FilterCascade::applyInPlace(array_span<float> wave) const {
	for (const auto& filterPtr : filters) {
		filterPtr->apply(wave);
	}
}
