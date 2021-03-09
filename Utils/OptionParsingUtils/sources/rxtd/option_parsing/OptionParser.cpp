// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include "OptionParser.h"

#include "rxtd/expression_parser/ASTSolver.h"
#include "rxtd/expression_parser/GrammarBuilder.h"

using rxtd::option_parsing::OptionParser;
using rxtd::expression_parser::ASTSolver;
using rxtd::expression_parser::ASTParser;
using rxtd::expression_parser::Lexer;

OptionParser OptionParser::getDefault() {
	OptionParser result;
	result.setGrammar(expression_parser::GrammarBuilder::makeSimpleMath());
	return result;
}

double OptionParser::parseFloatImpl(sview source, sview loggerPrefix) {
	try {
		parser.parse(source);

		ASTSolver solver{ parser.takeTree() };
		return solver.solve(nullptr);
	} catch (ASTSolver::Exception& e) {
		logger.error(L"{}: can't parse value as a number: {}, value: {}", loggerPrefix, e.getMessage(), source);
	} catch (Lexer::Exception& e) {
		logger.error(L"{}: can't parse value as a number: unknown token after '{}' and before '{}'", loggerPrefix, source.substr(0, e.getPosition()), source.substr(e.getPosition()));
	} catch (ASTParser::Exception& e) {
		if (e.getPosition() < source.length()) {
			logger.error(
				L"{}: can't parse value as a number: {}, after '{}' and before '{}'",
				loggerPrefix, e.getReason(), source.substr(0, e.getPosition()), source.substr(e.getPosition())
			);
		} else {
			logger.error(L"{}: can't parse value as a number: {}, value: {}", loggerPrefix, e.getReason(), source);
		}
	}

	throw Exception{};
}
