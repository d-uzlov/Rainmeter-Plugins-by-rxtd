/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <functional>
#include "AbstractFilter.h"
#include "option-parser/OptionList.h"
#include "FilterCascade.h"

namespace rxtd::audio_utils {
	class FilterCascadeCreator {
	public:
		using FilterCreationFunction = std::function<std::unique_ptr<AbstractFilter>(double sampleFrequency)>;

	private:
		string source;
		std::vector<FilterCreationFunction> patchers;

	public:
		FilterCascadeCreator() = default;

		FilterCascadeCreator(string source, std::vector<FilterCreationFunction> patchers) :
			source(source), patchers(std::move(patchers)) {
		}

		friend bool operator==(const FilterCascadeCreator& lhs, const FilterCascadeCreator& rhs) {
			return lhs.source == rhs.source;
		}

		friend bool operator!=(const FilterCascadeCreator& lhs, const FilterCascadeCreator& rhs) {
			return !(lhs == rhs);
		}

		FilterCascade getInstance(double samplingFrequency) const;
	};

	class FilterCascadeParser {
	public:
		using FCF = FilterCascadeCreator::FilterCreationFunction;

		static FilterCascadeCreator parse(const utils::Option& description);

	private:
		static FCF parseFilter(const utils::OptionList& description);

		static FCF parseBWLowPass(index order, double cutoff);

		static FCF parseBWHighPass(index order, double cutoff);

		static FCF parseBWBandPass(index order, double cutoffLow, double cutoffHigh);

		static FCF parseBWBandStop(index order, double cutoffLow, double cutoffHigh);
	};
}
