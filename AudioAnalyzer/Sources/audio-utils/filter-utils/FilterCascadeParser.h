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
#include "FilterCascade.h"
#include "rainmeter/Logger.h"
#include "../butterworth-lib/ButterworthWrapper.h"
#include "option-parsing/Option.h"

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
			source(std::move(source)), patchers(std::move(patchers)) { }

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
		
		using Logger = ::rxtd::common::rainmeter::Logger;
		using Option = ::rxtd::common::options::Option;
		using OptionList = ::rxtd::common::options::OptionList;
		using OptionMap = ::rxtd::common::options::OptionMap;

		[[nodiscard]]
		static FilterCascadeCreator parse(const Option& description, Logger& logger);

	private:
		[[nodiscard]]
		static FCF parseFilter(const OptionList& description, Logger& logger);

		[[nodiscard]]
		static FCF parseBQ(isview name, const OptionMap& description, Logger& cl);

		[[nodiscard]]
		static FCF parseBW(isview name, const OptionMap& description, Logger& cl);

		template<index size>
		[[nodiscard]]
		static FCF createButterworthMaker(
			index order,
			double forcedGain,
			double freq1, double freq2,
			const ButterworthWrapper::GenericCoefCalculator& butterworthMaker
		);

		template<ButterworthWrapper::SizeFuncSignature sizeFunc>
		[[nodiscard]]
		static FCF createButterworth(
			index order,
			double forcedGain,
			double freq1, double freq2,
			const ButterworthWrapper::GenericCoefCalculator& butterworthMaker
		);
	};
}
