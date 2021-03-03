/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionParser.h"

#include "rxtd/expression_parser/ASTSolver.h"
#include "rxtd/expression_parser/GrammarBuilder.h"

using rxtd::option_parsing::OptionParser;
using rxtd::expression_parser::ASTSolver;

OptionParser OptionParser::getDefault() {
	OptionParser result;
	result.setGrammar(expression_parser::GrammarBuilder::makeSimpleMath());
	return result;
}

double OptionParser::parseFloat(const Option& opt, double defaultValue) {
	sview source = opt.asString();
	if (source.empty()) {
		return defaultValue;
	}

	try {
		parser.parse(source);

		ASTSolver solver{ parser.takeTree() };
		return solver.solve(nullptr);
	} catch (ASTSolver::Exception& e) {
		logger.error(L"can't parse '{}' as a number: {}", source, e.getMessage());
	} catch (expression_parser::Lexer::Exception& e) {
		logger.error(L"can't parse '{}' as a number: unknown token here: '{}'", source, source.substr(e.getPosition()));
	} catch (expression_parser::ASTParser::Exception& e) {
		logger.error(L"can't parse '{}' as a number: {}, at position: '{}'", source, e.getReason(), source.substr(e.getPosition()));
	}

	return 0.0;
}

bool OptionParser::parseBool(const Option& opt, bool defaultValue) {
	const isview view = opt.asIString();
	if (view.empty()) {
		return defaultValue;
	}
	if (view == L"true") {
		return true;
	}
	if (view == L"false") {
		return false;
	}
	return parseFloat(opt) != 0.0;
}
