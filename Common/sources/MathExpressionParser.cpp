/*
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <cwctype>

#include "MathExpressionParser.h"

#include "StringUtils.h"

using namespace utils;

void ExpressionTreeNode::solve() {
	if (type == ExpressionType::eNUMBER) {
		return;
	}

	bool isConst = true;
	for (ExpressionTreeNode& node : nodes) {
		node.solve();
		if (node.type != ExpressionType::eNUMBER) {
			isConst = false;
		}
	}
	if (!isConst) {
		return;
	}

	switch (type) {
	case ExpressionType::eSUM:
	{
		double value = 0;
		for (ExpressionTreeNode& node : nodes) {
			value += node.number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::eNUMBER;
		return;
	}
	case ExpressionType::eDIFF:
	{
		double value = nodes[0].number;
		for (index i = 1; i < index(nodes.size()); i++) {
			value -= nodes[i].number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::eNUMBER;
		return;
	}
	case ExpressionType::eINVERSE:
	{
		number = -nodes[0].number;
		nodes.clear();
		type = ExpressionType::eNUMBER;
		return;
	}
	case ExpressionType::eMULT:
	{
		double value = 1;
		for (ExpressionTreeNode& node : nodes) {
			value *= node.number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::eNUMBER;
		return;
	}
	case ExpressionType::eDIV:
	{
		double value = nodes[0].number;
		for (index i = 1; i < index(nodes.size()); i++) {
			const double nodeValue = nodes[i].number;
			if (nodeValue == 0) {
				value = 0.0;
				break;
			}
			value /= nodeValue;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::eNUMBER;
		return;
	}
	case ExpressionType::ePOWER:
	{
		double value = nodes[0].number;
		value = std::pow(value, nodes[1].number);
		nodes.clear();
		number = value;
		type = ExpressionType::eNUMBER;
		return;
	}
	default:;
	}
}

MathExpressionParser::Lexer::Lexer(sview source) :
	source(source),
	sourceLength(static_cast<index>(source.length())) {
}

MathExpressionParser::Lexer::Lexeme MathExpressionParser::Lexer::next() {
	skipSpaces();

	if (position >= sourceLength) {
		return Lexeme(LexemeType::eEND, { });
	}

	if (isSymbol(source[position])) {
		const wchar_t value = source[position];
		LexemeType type = LexemeType::eUNKNOWN;
		position++;
		switch (value) {
		case L'(':
			type = LexemeType::ePAR_OPEN;
			break;
		case L')':
			type = LexemeType::ePAR_CLOSE;
			break;
		case L'[':
			type = LexemeType::eBRACKET_OPEN;
			break;
		case L']':
			type = LexemeType::eBRACKET_CLOSE;
			break;
		case L'+':
			type = LexemeType::ePLUS;
			break;
		case L'-':
			type = LexemeType::eMINUS;
			break;
		case L'*':
			type = LexemeType::eASTERISK;
			break;
		case L'/':
			type = LexemeType::eFORWARD_SLASH;
			break;
		case L'^':
			type = LexemeType::eCARET;
			break;
		case L'\0':
			type = LexemeType::eEND;
			break;
		case L'.':
			type = LexemeType::eDOT;
			break;
		case L'#':
			type = LexemeType::eHASH;
			break;
		default:
			type = LexemeType::eUNKNOWN;
			break;
		}
		return Lexeme(type, std::to_wstring(value));
	}

	if (std::iswdigit(source[position])) {
		return Lexeme(LexemeType::eNUMBER, readNumber());
	}

	Lexeme result(LexemeType::eUNKNOWN, std::to_wstring(source[position]));
	position++;
	return result;
}

sview MathExpressionParser::Lexer::getUntil(const wchar_t stop1, const wchar_t stop2) {
	const auto startPos = position;
	index i = 0;
	while (position + i < sourceLength) {
		const wchar_t c1 = source[position + i];
		if (c1 != stop1 && c1 != stop2 && c1 != L'\0') {
			i++;
		} else {
			break;
		}
	}
	position += i;
	return source.substr(startPos, i);
}

void MathExpressionParser::Lexer::skipSpaces() {
	while (position < sourceLength && std::iswspace(source[position])) {
		position++;
	}
}

bool MathExpressionParser::Lexer::isSymbol(const wchar_t c) {
	return
		c == L'\0' ||
		c == L'(' ||
		c == L')' ||
		c == L'+' ||
		c == L'-' ||
		c == L'*' ||
		c == L'/' ||
		c == L'^' ||
		c == L'[' ||
		c == L']' ||
		c == L'.' ||
		c == L'#';
}

sview MathExpressionParser::Lexer::readNumber() {
	const auto startPos = position;
	index i = 0;
	while (position + i < sourceLength) {
		const wchar_t c1 = source[position + i];
		if (std::iswdigit(c1)) {
			i++;
		} else {
			break;
		}
	}
	position += i;
	return source.substr(startPos, i);
}

MathExpressionParser::MathExpressionParser(string source) : source(std::move(source)), lexer(this->source) {
	readNext();
}

MathExpressionParser::MathExpressionParser(sview source) : lexer(source) {
	readNext();
}

void MathExpressionParser::parse() {
	result = parseExpression();
	if (next.type != Lexer::LexemeType::eEND) {
		error = true;
	}
}
bool MathExpressionParser::isError() const {
	return error;
}
ExpressionTreeNode MathExpressionParser::getExpression() const {
	return result;
}
void MathExpressionParser::readNext() {
	next = lexer.next();
	if (next.type == Lexer::LexemeType::eUNKNOWN) {
		error = true;
	}
}

ExpressionTreeNode MathExpressionParser::parseExpression() {
	ExpressionTreeNode result = parseTerm();
	if (error) {
		return ExpressionTreeNode();
	}
	while (next.type == Lexer::LexemeType::ePLUS || next.type == Lexer::LexemeType::eMINUS) {
		if (next.type == Lexer::LexemeType::ePLUS) {
			ExpressionTreeNode sumResult;
			sumResult.type = ExpressionType::eSUM;
			sumResult.nodes.push_back(result);
			while (next.type == Lexer::LexemeType::ePLUS) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseTerm();
				if (error) {
					return ExpressionTreeNode();
				}
				sumResult.nodes.push_back(result);
			}
			result = std::move(sumResult);
		}
		if (next.type == Lexer::LexemeType::eMINUS) {
			ExpressionTreeNode diffResult;
			diffResult.type = ExpressionType::eDIFF;
			diffResult.nodes.push_back(result);
			while (next.type == Lexer::LexemeType::eMINUS) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseTerm();
				if (error) {
					return ExpressionTreeNode();
				}
				diffResult.nodes.push_back(result);
			}
			result = std::move(diffResult);
		}
	}
	return result;
}

ExpressionTreeNode MathExpressionParser::parseTerm() {
	ExpressionTreeNode result = parseFactor();
	if (error) {
		return ExpressionTreeNode();
	}
	while (next.type == Lexer::LexemeType::eASTERISK || next.type == Lexer::LexemeType::eFORWARD_SLASH) {
		if (next.type == Lexer::LexemeType::eASTERISK) {
			ExpressionTreeNode multResult;
			multResult.type = ExpressionType::eMULT;
			multResult.nodes.push_back(result);
			while (next.type == Lexer::LexemeType::eASTERISK) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseFactor();
				if (error) {
					return ExpressionTreeNode();
				}
				multResult.nodes.push_back(result);
			}
			result = std::move(multResult);
		}
		if (next.type == Lexer::LexemeType::eFORWARD_SLASH) {
			ExpressionTreeNode divResult;
			divResult.type = ExpressionType::eDIV;
			divResult.nodes.push_back(result);
			while (next.type == Lexer::LexemeType::eFORWARD_SLASH) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseFactor();
				if (error) {
					return ExpressionTreeNode();
				}
				divResult.nodes.push_back(result);
			}
			result = std::move(divResult);
		}
	}
	return result;
}

ExpressionTreeNode MathExpressionParser::parseFactor() {
	ExpressionTreeNode power = parsePower();
	if (error) {
		return ExpressionTreeNode();
	}
	if (next.type == Lexer::LexemeType::eCARET) {
		ExpressionTreeNode res;
		res.type = ExpressionType::ePOWER;
		res.nodes.push_back(power);
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		power = parseFactor();
		if (error) {
			return ExpressionTreeNode();
		}
		res.nodes.push_back(power);
		return res;
	}
	return power;
}

ExpressionTreeNode MathExpressionParser::parsePower() {
	if (next.type == Lexer::LexemeType::eMINUS) {
		ExpressionTreeNode res;
		res.type = ExpressionType::eINVERSE;
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		const ExpressionTreeNode atom = parseAtom();
		if (error) {
			return ExpressionTreeNode();
		}
		res.nodes.push_back(atom);
		return res;
	}
	return parseAtom();
}

ExpressionTreeNode MathExpressionParser::parseAtom() {
	if (next.type == Lexer::LexemeType::ePAR_OPEN) {
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		ExpressionTreeNode res = parseExpression();
		if (next.type != Lexer::LexemeType::ePAR_CLOSE) {
			error = true;
			return ExpressionTreeNode();
		}
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		return res;
	}
	if (next.type == Lexer::LexemeType::eNUMBER) {
		ExpressionTreeNode res;
		res.type = ExpressionType::eNUMBER;
		const double i = double(parseInt(next.value));
		double m = 0;
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		if (next.type == Lexer::LexemeType::eDOT) {
			readNext();
			if (error) {
				return ExpressionTreeNode();
			}
			if (next.type != Lexer::LexemeType::eNUMBER) {
				error = true;
				return ExpressionTreeNode();
			}
			m = parseFractional(next.value);
			readNext();
			if (error) {
				return ExpressionTreeNode();
			}
		}
		res.number = i + m;
		return res;
	}
	error = true;
	return ExpressionTreeNode();
}

index MathExpressionParser::parseInt(sview view) {
	return StringUtils::parseInt(view);
}

double MathExpressionParser::parseFractional(sview view) {
	return StringUtils::parseFractional(view);
}

