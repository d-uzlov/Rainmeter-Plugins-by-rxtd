// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once
#include <functional>
#include "AbstractFilter.h"
#include "FilterCascade.h"
#include "rxtd/Logger.h"
#include "rxtd/filter_utils/butterworth_lib/ButterworthWrapper.h"
#include "rxtd/option_parsing/Option.h"
#include "rxtd/option_parsing/OptionParser.h"

namespace rxtd::filter_utils {
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

	class FilterParser { };

	class FilterCascadeParser {
		option_parsing::OptionParser& parser;

	public:
		using FCF = FilterCascadeCreator::FilterCreationFunction;

		using Option = option_parsing::Option;
		using OptionList = option_parsing::OptionList;
		using OptionMap = option_parsing::OptionMap;
		using ButterworthWrapper = butterworth_lib::ButterworthWrapper;

		FilterCascadeParser(option_parsing::OptionParser& parser) :
			parser(parser) {}

		// can throw ParamParser::Exception
		[[nodiscard]]
		FilterCascadeCreator parse(const Option& description, const Logger& logger);

	private:
		// can throw ParamParser::Exception
		[[nodiscard]]
		FCF parseFilter(const Option& nameOpt, const Option& argsOpt, const Logger& logger);

		// can throw ParamParser::Exception
		[[nodiscard]]
		FCF parseBQ(isview name, const OptionMap& description, const Logger& cl);

		// can throw ParamParser::Exception
		[[nodiscard]]
		FCF parseBW(isview name, const OptionMap& description, const Logger& cl);

		template<index size>
		[[nodiscard]]
		FCF createButterworthMaker(
			index order,
			double forcedGain,
			double freq1, double freq2,
			const ButterworthWrapper::GenericCoefCalculator& butterworthMaker
		);

		template<ButterworthWrapper::SizeFuncSignature sizeFunc>
		[[nodiscard]]
		FCF createButterworth(
			index order,
			double forcedGain,
			double freq1, double freq2,
			const ButterworthWrapper::GenericCoefCalculator& butterworthMaker
		);
	};
}
