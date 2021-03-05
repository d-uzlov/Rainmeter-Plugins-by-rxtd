// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#include "FilterCascade.h"

using rxtd::filter_utils::FilterCascade;

void FilterCascade::apply(array_view<float> wave) {
	wave.transferToVector(processed);
	applyInPlace(processed);
}

void FilterCascade::applyInPlace(array_span<float> wave) const {
	for (const auto& filterPtr : filters) {
		filterPtr->apply(wave);
	}
}
