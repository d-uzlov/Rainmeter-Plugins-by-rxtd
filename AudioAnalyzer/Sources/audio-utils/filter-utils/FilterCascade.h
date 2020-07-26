/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "InfiniteResponseFilter.h"

namespace rxtd::audio_utils {
	class FilterCascade {
		std::vector<std::unique_ptr<AbstractFilter>> filters;

		std::vector<float> processed;

	public:
		FilterCascade() = default;

		FilterCascade(std::vector<std::unique_ptr<AbstractFilter>> filters) :
			filters(std::move(filters)) {
		}

		void apply(array_view<float> wave);

		void applyInPlace(array_span<float> wave);

		array_view<float> getProcessed() const {
			return processed;
		}

		bool isEmpty() const {
			return filters.empty();
		}
	};
}
