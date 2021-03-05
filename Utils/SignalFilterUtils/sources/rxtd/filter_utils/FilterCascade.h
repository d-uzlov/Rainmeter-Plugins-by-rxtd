// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include "InfiniteResponseFilter.h"

namespace rxtd::filter_utils {
	class FilterCascade {
		std::vector<std::unique_ptr<AbstractFilter>> filters;

		std::vector<float> processed;

	public:
		FilterCascade() = default;

		FilterCascade(std::vector<std::unique_ptr<AbstractFilter>> filters) :
			filters(std::move(filters)) { }

		void apply(array_view<float> wave);

		void applyInPlace(array_span<float> wave) const;

		[[nodiscard]]
		array_view<float> getProcessed() const {
			return processed;
		}

		[[nodiscard]]
		bool isEmpty() const {
			return filters.empty();
		}
	};
}
