// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#pragma once

#include "Option.h"
#include "rxtd/Logger.h"
#include "rxtd/expression_parser/ASTParser.h"

namespace rxtd::option_parsing {
	class OptionParser {
		Logger logger;
		expression_parser::ASTParser parser;

		OptionParser() = default;

	public:
		static OptionParser getDefault();

		void setGrammar(const expression_parser::GrammarDescription& grammar) {
			parser.setGrammar(grammar, false);
		}

		void setLogger(Logger value) {
			logger = std::move(value);
		}

		// Parse float, support math operations.
		[[nodiscard]]
		double parseFloat(const Option& opt, double defaultValue = 0.0);

		// Parse float, support math operations.
		[[nodiscard]]
		float parseFloatF(const Option& opt, float defaultValue = 0.0) {
			return static_cast<float>(parseFloat(opt, static_cast<double>(defaultValue)));
		}

		// Parse integer value, support math operations.
		template<typename IntType = int32_t>
		IntType parseInt(const Option& opt, IntType defaultValue = 0) {
			static_assert(std::is_integral<IntType>::value);

			const auto dVal = parseFloat(opt, static_cast<double>(defaultValue));
			if (dVal > static_cast<double>(std::numeric_limits<IntType>::max()) ||
				dVal < static_cast<double>(std::numeric_limits<IntType>::lowest())) {
				return defaultValue;
			}
			return static_cast<IntType>(dVal);
		}

		[[nodiscard]]
		bool parseBool(const Option& opt, bool defaultValue = false);
	};
}
