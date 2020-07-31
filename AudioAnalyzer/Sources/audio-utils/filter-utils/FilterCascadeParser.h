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
#include <utility>
#include "AbstractFilter.h"
#include "FilterCascade.h"
#include "RainmeterWrappers.h"

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
			source(std::move(source)), patchers(std::move(patchers)) {
		}

		friend bool operator==(const FilterCascadeCreator& lhs, const FilterCascadeCreator& rhs) {
			return lhs.source == rhs.source;
		}

		friend bool operator!=(const FilterCascadeCreator& lhs, const FilterCascadeCreator& rhs) {
			return !(lhs == rhs);
		}

		[[nodiscard]]
		FilterCascade getInstance(double samplingFrequency) const;
	};

	class FilterCascadeParser {
	public:
		using FCF = FilterCascadeCreator::FilterCreationFunction;
		using ButterworthParamsFunc = FilterParameters(*)(
			index order, double samplingFrequency, double freq1, double freq2
		);

		[[nodiscard]]
		static FilterCascadeCreator parse(const utils::Option& description, utils::Rainmeter::Logger& logger);

	private:
		[[nodiscard]]
		static FCF parseFilter(const utils::OptionList& description, utils::Rainmeter::Logger& logger);

		[[nodiscard]]
		static FCF parseBQ(isview name, const utils::OptionMap& description, utils::Rainmeter::Logger& cl);

		[[nodiscard]]
		static FCF parseBW(isview name, const utils::OptionMap& description, utils::Rainmeter::Logger& cl);

		[[nodiscard]]
		static FCF createButterworth(
			index order,
			double forcedGain,
			double freq1, double freq2,
			ButterworthParamsFunc func
		);
	};
}
