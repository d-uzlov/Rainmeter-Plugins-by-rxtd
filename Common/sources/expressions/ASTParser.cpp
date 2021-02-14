/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ASTParser.h"

#include "StringUtils.h"

using namespace common::expressions;

void ASTParser::setGrammar(const GrammarDescription& grammarDescription, bool allowFreeFunctions) {
	functionParsingIsAllowed = allowFreeFunctions;

	std::set<sview> operatorSymbolsSet;

	for (auto info : grammarDescription.operators) {
		if (info.getType() == OperatorInfo::Type::ePREFIX) {
			opsInfoPrefix.emplace_back(info);
		} else {
			opsInfoNonPrefix.emplace_back(info);
		}
		operatorSymbolsSet.insert(info.getMainInfo().operatorValue);
	}

	groupingOperators = grammarDescription.groupingOperators;

	for (auto [first, second, separator] : groupingOperators) {
		operatorSymbolsSet.insert(first);
		operatorSymbolsSet.insert(second);
		operatorSymbolsSet.insert(separator);
	}

	operatorSymbols.assign(operatorSymbolsSet.begin(), operatorSymbolsSet.end());

	// Without sort by length there may be issues
	// with operators with matching beginning.
	// For example: '< 'and '<='
	// If '<' will be before '<=', them Lexer will always pick '<'
	// even when there would be '=' after '<'
	std::sort(operatorSymbols.begin(), operatorSymbols.end(), [](sview s1, sview s2) { return s1.length() > s2.length(); });

	lexer.setStandardSymbols(operatorSymbols);
}

void ASTParser::parse(sview source) {
	tree.clear();
	tree.setSource(source);

	lexer.setSource(tree.getSource());
	skipToken();

	parseBinaryOrPostfix(0);

	if (next.type != Lexer::Type::eEND) {
		throwException(L"End of expression was expected but was not found");
	}
}

ast_nodes::IndexType ASTParser::parseBinaryOrPostfix(index minAllowedPrecedence) {
	auto node = parsePrefixOrTerminalOrGroup();
	index maxAllowedPrecedence = std::numeric_limits<index>::max();

	while (true) {
		if (next.type != Lexer::Type::eSYMBOL) {
			return node;
		}

		if (auto pairOpt = tryParseGroupingOperator(next.value);
			pairOpt.has_value()) {
			return node;
		}

		const auto info = parseOperator(next.value);
		const bool precedenceIsAllowed = minAllowedPrecedence <= info.getMainPrecedence() && info.getMainPrecedence() <= maxAllowedPrecedence;
		if (!precedenceIsAllowed) {
			return node;
		}

		switch (info.getType()) {
		case OperatorInfo::Type::eBINARY: {
			skipToken();
			auto const rightNode = parseBinaryOrPostfix(info.getRightPrecedence());
			node = tree.allocateNode<ast_nodes::BinaryOperatorNode>(info.getMainInfo(), node, rightNode);
			break;
		}
		case OperatorInfo::Type::ePOSTFIX: {
			skipToken();
			node = tree.allocateNode<ast_nodes::PostfixOperatorNode>(info.getMainInfo(), node);
			break;
		}
		case OperatorInfo::Type::ePREFIX: {
			return node;
		}
		}

		maxAllowedPrecedence = info.getNextPrecedence();
	}
}

ast_nodes::IndexType ASTParser::parsePrefixOrTerminalOrGroup() {
	switch (next.type) {
	case Lexer::Type::eEND: {
		throwException(L"Unexpected end of expression");
	}
	case Lexer::Type::eNUMBER: {
		const double number = utils::StringUtils::parseFloat(next.value);
		auto const result = tree.allocateNode<ast_nodes::ConstantNode>(number);
		skipToken();
		return result;
	}
	case Lexer::Type::eSYMBOL: {
		if (auto pairOpt = tryParseGroupingOperator(next.value);
			pairOpt.has_value()) {
			auto [p1, p2, sep] = pairOpt.value();

			if (next.value != p1) {
				throwException(L"Unexpected closing grouping operator");
			}

			skipToken();
			auto const result = parseBinaryOrPostfix(0);

			if (next.value != p2) {
				throwException(L"Expected closing grouping operator but something else was found");
			}

			skipToken();

			return result;
		}

		auto info = parseOperator(next.value, true);
		if (info.getType() != OperatorInfo::Type::ePREFIX) {
			throwException(L"Expected unary prefix operator or grouping operator but something else was found");
		}

		skipToken();
		return tree.allocateNode<ast_nodes::PrefixOperatorNode>(info.getMainInfo(), parseBinaryOrPostfix(info.getMainPrecedence()));
	}
	case Lexer::Type::eWORD: {
		auto word = next.value;

		auto indOpt = parseCustom();
		if (indOpt.has_value()) {
			return indOpt.value();
		}

		if (!functionParsingIsAllowed) {
			throwException(L"Unexpected word token");
		}

		skipToken();

		if (auto groupOpt = tryParseGroupingOperator(next.value);
			groupOpt.has_value() && groupOpt.value().begin == next.value) {
			skipToken();

			if (next.value == groupOpt->end) {
				skipToken();

				return tree.allocateNode<ast_nodes::FunctionNode>(word);
			}

			std::vector<ast_nodes::IndexType> children;
			while (true) {
				auto const functionArg = parseBinaryOrPostfix(0);
				children.push_back(functionArg);

				if (next.value == groupOpt.value().end) {
					break;
				}

				if (next.value != groupOpt.value().separator) {
					throwException(L"Expected closing grouping operator or separator but something else was found");
				}

				skipToken();
			}

			skipToken();

			return tree.allocateNode<ast_nodes::FunctionNode>(word, std::move(children));
		} else {
			return tree.allocateNode<ast_nodes::WordNode>(word);
		}
	}
	}
	throwException(L"Unexpected token type");
}

ASTParser::OperatorInfo ASTParser::parseOperator(sview value, bool prefixFirst) const {
	auto firstList = opsInfoNonPrefix;
	auto secondList = opsInfoPrefix;
	if (prefixFirst) {
		std::swap(firstList, secondList);
	}


	auto iter = std::find_if(
		firstList.begin(), firstList.end(), [=](OperatorInfo info) {
			return value == info.getMainInfo().operatorValue;
		}
	);

	if (iter != firstList.end()) {
		return *iter;
	}


	iter = std::find_if(
		secondList.begin(), secondList.end(), [=](OperatorInfo info) {
			return value == info.getMainInfo().operatorValue;
		}
	);

	if (iter != secondList.end()) {
		return *iter;
	}

	throwException(L"Expected operator but something else was found");
}

std::optional<ASTParser::GroupingOperatorInfo> ASTParser::tryParseGroupingOperator(sview value) const {
	for (auto pair : groupingOperators) {
		if (pair.begin == value || pair.end == value || pair.separator == value) {
			return pair;
		}
	}

	return {};
}
