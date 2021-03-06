// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include "ExpressionParser.h"


#include "MatchPattern.h"
#include "Reference.h"
#include "rxtd/expression_parser/GrammarBuilder.h"
#include "rxtd/std_fixes/StringUtils.h"

using rxtd::perfmon::ExpressionParser;
using StringUtils = rxtd::std_fixes::StringUtils;

ExpressionParser::ExpressionParser() {
	setGrammar(expression_parser::GrammarBuilder::makeSimpleMath(), false);
}

std::optional<ExpressionParser::IndexType> ExpressionParser::parseCustom() {
	using Type = Reference::Type;

	Reference ref;
	const isview name = next.value % ciView();
	if (name == L"counterRaw" || name == L"CR") {
		ref.type = Type::eCOUNTER_RAW;
	} else if (name == L"counterFormated" || name == L"counterFormatted" || name == L"CF") {
		ref.type = Type::eCOUNTER_FORMATTED;
	} else if (name == L"expression" || name == L"E") {
		ref.type = Type::eEXPRESSION;
	} else if (name == L"rollupExpression" || name == L"R") {
		ref.type = Type::eROLLUP_EXPRESSION;
	} else if (name == L"count" || name == L"C") {
		ref.type = Type::eCOUNT;
	} else {
		return {};
	}

	sview additionalSymbols[] = {
		L"[",
		L"]",
	};

	skipToken(additionalSymbols);

	if (ref.type != Type::eCOUNT) {
		if (next.type != Lexer::Type::eNUMBER) {
			throwException(L"Expected number but something else was found");
		}

		ref.counter = StringUtils::parseInt(next.value);
		skipToken(additionalSymbols);
	}

	if (next.value == L"[") {
		readUntilAnyOf(L"#]");
		sview description = next.value;
		if (!description.empty()) {
			if (description.front() == L'\\') {
				auto indexOfFirstNonFlag = description.find_first_of(L" \t");
				if (indexOfFirstNonFlag == std::string::npos) {
					indexOfFirstNonFlag = description.length();
				}
				const isview flags = description.substr(1, indexOfFirstNonFlag - 1) % ciView();

				if (flags.find(L'D') != std::string::npos) {
					ref.discarded = true;
				}
				if (flags.find(L'O') != std::string::npos) {
					ref.useOrigName = true;
				}
				if (flags.find(L'T') != std::string::npos) {
					ref.total = true;
				}

				description.remove_prefix(indexOfFirstNonFlag);
			}

			description = StringUtils::trim(description);

			ref.namePattern = MatchPattern{ description };
		}

		sview closingBracket = L"]";
		skipToken(array_view<sview>{ &closingBracket, 1 });
		if (next.type != Lexer::Type::eSYMBOL || next.value != L"]") {
			throwException(L"Expected ']' but something else was found");
		}
		skipToken();
	}

	if (next.type == Lexer::Type::eWORD) {
		const isview suffix = next.value % ciView();
		if (suffix == L"SUM" || suffix == L"S") {
			ref.rollupFunction = RollupFunction::eSUM;
		} else if (suffix == L"AVG" || suffix == L"A") {
			ref.rollupFunction = RollupFunction::eAVERAGE;
		} else if (suffix == L"MIN" || next.value == L"m") {
			ref.rollupFunction = RollupFunction::eMINIMUM;
		} else if (suffix == L"MAX" || next.value == L"M") {
			ref.rollupFunction = RollupFunction::eMAXIMUM;
		} else if (suffix == L"COUNT" || suffix == L"C") {
			// handling deprecated rollup function
			ref.type = Type::eCOUNT;
		} else if (suffix == L"FIRST" || suffix == L"F") {
			ref.rollupFunction = RollupFunction::eFIRST;
		}
		skipToken();
	}

	return tree.allocateNode<expression_parser::ast_nodes::CustomTerminalNode>(ref);
}
