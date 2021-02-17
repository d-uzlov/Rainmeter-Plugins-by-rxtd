/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ExpressionParser.h"

#include "Reference.h"
#include "StringUtils.h"
#include "expressions/GrammarBuilder.h"

using namespace rxtd::perfmon;

ExpressionParser::ExpressionParser() {
	setGrammar(common::expressions::GrammarBuilder::makeSimpleMath(), false);
}

std::optional<ExpressionParser::IndexType> ExpressionParser::parseCustom() {
	using Type = Reference::Type;
	
	Reference ref;
	const isview name = next.value % ciView();
	if (name == L"counterRaw" || name == L"CR") {
		ref.type = Type::COUNTER_RAW;
	} else if (name == L"counterFormated" || name == L"counterFormatted" || name == L"CF") {
		ref.type = Type::COUNTER_FORMATTED;
	} else if (name == L"expression" || name == L"E") {
		ref.type = Type::EXPRESSION;
	} else if (name == L"rollupExpression" || name == L"R") {
		ref.type = Type::ROLLUP_EXPRESSION;
	} else if (name == L"count" || name == L"C") {
		ref.type = Type::COUNT;
	} else {
		return {};
	}

	sview additionalSymbols[] = {
		L"[",
		L"]",
	};

	skipToken(additionalSymbols);

	if (ref.type != Type::COUNT) {
		if (next.type != Lexer::Type::eNUMBER) {
			throwException(L"Expected number but something else was found");
		}

		ref.counter = utils::StringUtils::parseInt(next.value);
		skipToken(additionalSymbols);
	}
	if (next.value == L"[") {
		readUntil(L"#]");
		ref.name = next.value;
		ref.named = !ref.name.empty();
		if (ref.named) {
			if (ref.name[0] == L'\\') {
				auto indexOfFirstNonFlag = ref.name.find_first_of(L" \t");
				if (indexOfFirstNonFlag == std::string::npos) {
					indexOfFirstNonFlag = ref.name.length();
				}
				const isview flags = (ref.name % ciView()).substr(1, indexOfFirstNonFlag - 1);

				if (flags.find(L'D') != std::string::npos) {
					ref.discarded = true;
				}
				if (flags.find(L'O') != std::string::npos) {
					ref.useOrigName = true;
				}
				if (flags.find(L'T') != std::string::npos) {
					ref.total = true;
				}
				utils::StringUtils::substringInplace(ref.name, indexOfFirstNonFlag);
			}

			utils::StringUtils::trimInplace(ref.name);

			const auto len = ref.name.size();
			if (len >= 2 && ref.name[0] == L'*' && ref.name[len - 1] == L'*') {
				ref.name = ref.name.substr(1, len - 2);
				ref.namePartialMatch = true;
			}
		}
		skipToken(additionalSymbols);
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
			ref.type = Type::COUNT;
		} else if (suffix == L"FIRST" || suffix == L"F") {
			ref.rollupFunction = RollupFunction::eFIRST;
		}
		skipToken();
	}
	return tree.allocateNode<common::expressions::ast_nodes::CustomTerminalNode>(std::move(ref));
}
