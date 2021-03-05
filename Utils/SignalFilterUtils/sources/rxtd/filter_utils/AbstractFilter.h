// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd::filter_utils {
	class AbstractFilter {
	public:
		virtual ~AbstractFilter() = default;

		virtual void apply(array_span<float> signal) = 0;

		virtual void addGainDbEnergy(double gainDB) = 0;
	};
}
